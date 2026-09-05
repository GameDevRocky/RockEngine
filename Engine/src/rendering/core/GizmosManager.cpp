#include "engine/rendering/core/GizmosManager.hpp"
#include "Engine.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/components/Camera.hpp"
#include "engine/components/ParticleComponent.hpp"
#include "engine/components/Light.hpp"
#include "engine/components/ShadowCaster.hpp"
#include "engine/components/AudioSource.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/Joint.hpp"
#include "engine/components/RevoluteJoint.hpp"
#include "engine/components/PrismaticJoint.hpp"
#include "engine/components/WheelJoint.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/core/Scene.hpp"
#include "engine/rendering/Renderer.hpp"
#include "engine/rendering/views/GameRenderView.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>

#include "engine/rendering/core/GridSettings.hpp"
#include "engine/rendering/core/GizmoSettings.hpp"

GizmosManager* GizmosManager::instance = nullptr;

namespace {
    // Grid-snap increments now live in GridSettings so the toolbar can drive
    // them, and so the translate step stays tied to the grid actually drawn.
    // Scale still snaps the drag RATIO (1.0-relative), never the absolute scale,
    // so it can't collapse.
    float MoveSnapStep()   { return GridSettings::Get().GetMoveSnap(); }
    float RotateSnapDeg()  { return GridSettings::Get().GetRotateSnap(); }
    float ScaleSnapStep()  { return GridSettings::Get().GetScaleSnap(); }

    // Thresholds that decide which single operation a drag frame is exercising.
    constexpr float kPosActiveEps = 0.25f;   // world units
    constexpr float kRotActiveEps = 0.05f;   // degrees

    float SnapTo(float v, float step) { return std::round(v / step) * step; }
    glm::vec2 SnapTo(const glm::vec2& v, float step) { return { SnapTo(v.x, step), SnapTo(v.y, step) }; }

    // Is `p` inside the convex quad q0..q3 (screen space, either winding)?
    // The point is inside when it falls on the same side of all four edges, so
    // the cross products must not disagree in sign. Accepts either winding, which
    // matters because a mirrored (negative-scale) sprite reverses it.
    bool PointInQuad(const ImVec2& p, const ImVec2 q[4]) {
        bool anyNeg = false, anyPos = false;
        for (int i = 0; i < 4; ++i) {
            const ImVec2& a = q[i];
            const ImVec2& b = q[(i + 1) % 4];
            const float cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
            if (cross < 0.0f) anyNeg = true;
            if (cross > 0.0f) anyPos = true;
            if (anyNeg && anyPos) return false;
        }
        return true;
    }
}

void GizmosManager::Init(){
}

void GizmosManager::Update(){
}

void GizmosManager::Shutdown(){
}

GizmosManager* GizmosManager::Copy() { return nullptr; }
GizmosManager* GizmosManager::Copy(Container* /*container*/) { return nullptr; }

void GizmosManager::SetOperation(ImGuizmo::OPERATION op) {
    if (m_currentOperation == op) return;
    m_currentOperation = op;
    Notify(TOOL_CHANGED_EVENT);
}

void GizmosManager::SetEditMode(EditMode mode) {
    if (m_editMode == mode) return;
    m_editMode = mode;
    Notify(TOOL_CHANGED_EVENT);
}

void GizmosManager::DrawGizmos(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight) {
    m_hoveredHandle = -1;        // reset each frame; DrawBoxColliderGizmo sets it if applicable
    m_hoveredCameraCorner = -1;  // reset each frame; DrawCameraGizmo sets it if applicable
    m_hoveredLightHandle = -1;   // reset each frame; DrawLightGizmos sets it if applicable
    m_hoveredAudioHandle = -1;   // reset each frame; DrawAudioSourceGizmos sets it if applicable
    m_hoveredScaleHandle = -1;   // reset each frame; DrawSpriteBoxGizmo sets it if applicable

    // Overlay visibility is per-category, driven by the Scene view's gizmo
    // dropdown. Read AFTER the hover resets above, never before: WantsCaptureMouse
    // ORs every m_hovered* field and SceneViewGui consults it before picking, so a
    // stale hover id left on a hidden gizmo would silently eat click-to-select.
    //
    // Hiding a category mid-drag also has to END that drag, and ending it means
    // COMMITTING it, not dropping it. The commit handlers live inside the draw
    // functions, so a skipped draw would otherwise strand m_drag*Handle latched
    // forever AND lose the edit: a drag writes its target every frame for live
    // feedback, so by the time the category is hidden the light's range (or the
    // camera's orthoSize, or the source's distances) has already been changed in
    // the world. Discarding the record would leave that change in place with
    // nothing on the undo stack to take it back -- strictly worse than either
    // committing or never having started.
    //
    // Emit before clearing the state the diff is taken against -- the same order
    // DrawColliderGizmo's release path uses. Each Commit*Drag re-resolves its
    // target through the Registry and no-ops when nothing actually moved, so a
    // grab-without-drag still records nothing.
    using Category = GizmoSettings::Category;
    const GizmoSettings& gizmoSettings = GizmoSettings::Get();

    // Camera view-region rects, light shapes and component icons are drawn for
    // every object regardless of selection, so they come first -- before the
    // no-selection early-return below.
    if (gizmoSettings.ShouldDraw(Category::Cameras)) {
        DrawCameraGizmos(view, proj, viewWidth, viewHeight);
    } else {
        if (m_dragCameraCorner >= 0 && !m_dragCameraId.empty()) CommitCameraDrag();
        m_dragCameraCorner = -1;
        m_dragCameraId.clear();
    }

    if (gizmoSettings.ShouldDraw(Category::Lights)) {
        DrawLightGizmos(view, proj, viewWidth, viewHeight);
    } else {
        if (m_dragLightHandle >= 0 && !m_dragLightId.empty()) CommitLightDrag();
        m_dragLightHandle = -1;
        m_dragLightId.clear();
    }

    if (gizmoSettings.ShouldDraw(Category::AudioSources)) {
        DrawAudioSourceGizmos(view, proj, viewWidth, viewHeight);
    } else {
        if (m_dragAudioHandle >= 0 && !m_dragAudioId.empty()) CommitAudioSourceDrag();
        m_dragAudioHandle = -1;
        m_dragAudioId.clear();
    }

    // Read-only from here down -- no drag state to release.
    if (gizmoSettings.ShouldDraw(Category::ComponentIcons))
        DrawComponentIcons(view, proj, viewWidth, viewHeight);

    Container* container = Engine::Get()->GetActiveContainer();
    SelectionManager* selectionManager = container->FindSystem<SelectionManager>();
    if (!selectionManager->HasSelection()) {
        // Selection cleared mid-drag: the per-gizmo release handlers below never
        // run, so drop the drag state here or m_transformGizmoActive stays stuck
        // true and the next drag would diff against a stale start value.
        m_dragHandle = -1;
        m_dragColliderId.clear();
        m_transformGizmoActive = false;
        m_dragRecords.clear();
        m_dragScaleHandle = -1;
        m_dragScaleId.clear();
        return;
    }

    // Selection-only, and independent of edit mode -- a joint's anchors are worth
    // seeing whichever tool is active, and without them anchor offsets are blind
    // numeric entry.
    if (gizmoSettings.ShouldDraw(Category::Joints))
        DrawJointGizmos(view, proj, viewWidth, viewHeight);

    // Likewise independent of edit mode: a "this is selected" outline, not a tool.
    if (gizmoSettings.ShouldDraw(Category::SelectionOutlines))
        DrawSelectionOutlines(view, proj, viewWidth, viewHeight);

    // The manipulators below are deliberately NOT gated: they are tools, not
    // overlays, and hiding the handle you are dragging is never what "hide
    // gizmos" is meant to mean.

    if (m_editMode == EditMode::Collider) {
        DrawColliderGizmo(view, proj, viewWidth, viewHeight);
    } else if (m_editMode == EditMode::SpriteBox) {
        DrawSpriteBoxGizmo(view, proj, viewWidth, viewHeight);
    } else {
        DrawTransformGizmo(view, proj, viewWidth, viewHeight);
    }
}

// ─── Sprite box-edit gizmo (EditMode::SpriteBox) ────────────────────────────
//
// Gives a sprite the same direct box editing a BoxCollider has: eight handles to
// resize, and a draggable body to move. It writes the TRANSFORM, because a
// sprite's world size is its pixel size times the transform's scale -- there is
// no per-sprite size field to edit.
//
// The whole drag is solved in the object's "box frame": the frame reached by the
// parent's world matrix, this object's translation and its rotation, but NOT its
// scale. Two reasons it has to be that frame and has to be captured once:
//   * scale is excluded, so the box's extent in it is exactly
//     localScale * spriteSize -- a plain ratio, with no matrix inversion needed
//     to recover the new scale;
//   * a resize writes position too (the opposite edge stays anchored), so
//     rebuilding the frame each drag frame would feed the edit back into its own
//     input and the box would slide away under the cursor.
void GizmosManager::DrawSpriteBoxGizmo(const glm::mat4& view, const glm::mat4& proj,
                                       float viewWidth, float viewHeight) {
    Container* container = Engine::Get()->GetActiveContainer();
    SelectionManager* selectionManager = container ? container->FindSystem<SelectionManager>() : nullptr;
    GameObject* selectedObj = selectionManager
        ? dynamic_cast<GameObject*>(selectionManager->GetSerializable()) : nullptr;
    if (!selectedObj) return;

    Transform* transform = selectedObj->GetComponent<Transform>();
    SpriteRenderer* renderer = selectedObj->GetComponent<SpriteRenderer>();
    if (!transform || !renderer) {
        // Nothing to box-edit -- fall back rather than leaving the viewport bare.
        DrawTransformGizmo(view, proj, viewWidth, viewHeight);
        return;
    }
    Sprite* sprite = renderer->GetSprite();
    if (!sprite) { DrawTransformGizmo(view, proj, viewWidth, viewHeight); return; }

    const glm::vec2 spriteSize = EngineUtils::RenderUtils::PixelsToWorld(sprite->GetPixelSize());
    if (spriteSize.x <= 0.0f || spriteSize.y <= 0.0f) {
        DrawTransformGizmo(view, proj, viewWidth, viewHeight);
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const glm::mat4 vp = proj * view;

    // Release ends the gesture. Centralised here, like every other gizmo.
    if (!io.MouseDown[0]) {
        if (m_dragScaleHandle >= 0 && !m_dragScaleId.empty()) CommitSpriteBoxDrag();
        m_dragScaleHandle = -1;
        m_dragScaleId.clear();
    }

    // The sprite quad in MODEL space, mirroring sprite.glsl's vertex stage:
    //     scaledPos = aPos * uSize - uSize * uPivot,  aPos in [-0.5, 0.5]
    // so it is centred at -uSize * pivot with half-extent uSize/2.
    const glm::vec2 quadHalf   = spriteSize * 0.5f;
    const glm::vec2 quadCenter = -spriteSize * sprite->GetPivot();

    const glm::mat4 worldMat = transform->GetWorldMatrix();
    const glm::vec2 localCorners[4] = {
        quadCenter + glm::vec2(-quadHalf.x, -quadHalf.y),   // 0 BL
        quadCenter + glm::vec2( quadHalf.x, -quadHalf.y),   // 1 BR
        quadCenter + glm::vec2( quadHalf.x,  quadHalf.y),   // 2 TR
        quadCenter + glm::vec2(-quadHalf.x,  quadHalf.y),   // 3 TL
    };

    ImVec2 screenCorners[4];
    for (int i = 0; i < 4; ++i)
        screenCorners[i] = WorldToScreen(
            glm::vec2(worldMat * glm::vec4(localCorners[i], 0.0f, 1.0f)), vp, viewWidth, viewHeight);

    // Light sky blue (#87CEFA).
    const ImU32 outlineColor = IM_COL32(135, 206, 250, 220);
    const ImU32 fillColor    = IM_COL32(135, 206, 250, 28);
    const ImU32 handleColor  = IM_COL32(135, 206, 250, 255);
    const ImU32 hotColor     = IM_COL32(225, 245, 255, 255);
    constexpr float kBoxHandlePx    = 5.0f;
    constexpr float kBoxHandleHitPx = kBoxHandlePx + 4.0f;

    // Faint fill doubles as the move affordance: it shows the grabbable body.
    drawList->AddQuadFilled(screenCorners[0], screenCorners[1],
                            screenCorners[2], screenCorners[3], fillColor);
    for (int i = 0; i < 4; ++i)
        drawList->AddLine(screenCorners[i], screenCorners[(i + 1) % 4], outlineColor, 2.0f);

    // 0-3 corners, 4-7 edge midpoints -- same layout as DrawBoxColliderGizmo.
    ImVec2 handles[8];
    for (int i = 0; i < 4; ++i) handles[i] = screenCorners[i];
    for (int i = 0; i < 4; ++i)
        handles[i + 4] = ImVec2((screenCorners[i].x + screenCorners[(i + 1) % 4].x) * 0.5f,
                                (screenCorners[i].y + screenCorners[(i + 1) % 4].y) * 0.5f);

    int hovered = -1;
    if (m_dragScaleHandle < 0) {
        for (int i = 0; i < 8; ++i) {
            const float dx = io.MousePos.x - handles[i].x;
            const float dy = io.MousePos.y - handles[i].y;
            if (dx * dx + dy * dy < kBoxHandleHitPx * kBoxHandleHitPx) { hovered = i; break; }
        }
        // Body = handle 8. Tested only after the handles, so a grip on an edge or
        // corner always wins over the move.
        if (hovered < 0 && PointInQuad(io.MousePos, screenCorners)) hovered = 8;
    }
    if (hovered >= 0) m_hoveredScaleHandle = hovered;

    if (io.MouseClicked[0] && hovered >= 0 && !ImGuizmo::IsUsing() &&
        m_dragHandle < 0 && m_dragCameraCorner < 0 && m_dragLightHandle < 0 &&
        m_dragAudioHandle < 0 && m_dragScaleHandle < 0) {
        // Box frame = parentWorld * T * R (this object's scale deliberately left out).
        glm::mat4 parentWorld(1.0f);
        if (Transform* parent = transform->GetParent()) parentWorld = parent->GetWorldMatrix();
        const glm::mat4 boxMat =
            parentWorld
            * glm::translate(glm::mat4(1.0f), glm::vec3(transform->localPosition, 0.0f))
            * glm::rotate(glm::mat4(1.0f), glm::radians(transform->localRotation), glm::vec3(0, 0, 1));

        m_dragScaleHandle        = hovered;
        m_dragScaleId            = transform->GetID();
        m_scaleDragStartScale    = transform->localScale;
        m_scaleDragStartLocalPos = transform->localPosition;
        m_scaleDragInvBox        = glm::inverse(boxMat);
        m_scaleDragStartBoxMouse = glm::vec2(m_scaleDragInvBox *
            glm::vec4(ScreenToWorld(io.MousePos, vp, viewWidth, viewHeight), 0.0f, 1.0f));
    }

    if (m_dragScaleHandle >= 0 && m_dragScaleId == transform->GetID() && io.MouseDown[0]) {
        const glm::vec2 mouseWorld = ScreenToWorld(io.MousePos, vp, viewWidth, viewHeight);
        const glm::vec2 boxMouse = glm::vec2(m_scaleDragInvBox * glm::vec4(mouseWorld, 0.0f, 1.0f));
        const glm::vec2 deltaBox = boxMouse - m_scaleDragStartBoxMouse;

        if (m_dragScaleHandle == 8) {
            // Body drag: pure translation. deltaBox is a box-frame vector and
            // localPosition is a parent-frame one, so it only needs the rotation
            // applied (translation doesn't affect vectors).
            const float r = glm::radians(transform->localRotation);
            const float c = std::cos(r), s = std::sin(r);
            const glm::vec2 deltaLocal(deltaBox.x * c - deltaBox.y * s,
                                       deltaBox.x * s + deltaBox.y * c);
            glm::vec2 newPos = m_scaleDragStartLocalPos + deltaLocal;
            if (m_snapRequested) newPos = SnapTo(newPos, MoveSnapStep());
            transform->SetPosition(newPos);
        } else {
            // Resize. In the box frame the quad's rect is startScale * (quadCenter
            // +/- quadHalf), so editing min/max there and dividing by spriteSize
            // hands back the new localScale directly.
            const glm::vec2 startCenter = m_scaleDragStartScale * quadCenter;
            const glm::vec2 startHalf   = glm::abs(m_scaleDragStartScale) * quadHalf;
            const float startMinX = startCenter.x - startHalf.x;
            const float startMaxX = startCenter.x + startHalf.x;
            const float startMinY = startCenter.y - startHalf.y;
            const float startMaxY = startCenter.y + startHalf.y;

            float minX = startMinX, maxX = startMaxX;
            float minY = startMinY, maxY = startMaxY;

            // corners: 0=BL 1=BR 2=TR 3=TL | edges: 4=bottom 5=right 6=top 7=left
            switch (m_dragScaleHandle) {
                case 0: minX += deltaBox.x; minY += deltaBox.y; break;
                case 1: maxX += deltaBox.x; minY += deltaBox.y; break;
                case 2: maxX += deltaBox.x; maxY += deltaBox.y; break;
                case 3: minX += deltaBox.x; maxY += deltaBox.y; break;
                case 4: minY += deltaBox.y; break;
                case 5: maxX += deltaBox.x; break;
                case 6: maxY += deltaBox.y; break;
                case 7: minX += deltaBox.x; break;
            }

            // Alt: aspect-preserving, anchored on the opposite handle -- same rule
            // and the same anchor table as the box collider gizmo.
            if (m_uniformScale) {
                const glm::vec2 startSize = glm::vec2(startMaxX - startMinX, startMaxY - startMinY);
                const float fx = (startSize.x > 1e-4f) ? (maxX - minX) / startSize.x : 1.0f;
                const float fy = (startSize.y > 1e-4f) ? (maxY - minY) / startSize.y : 1.0f;
                float f = (std::abs(fx - 1.0f) >= std::abs(fy - 1.0f)) ? fx : fy;
                f = std::max(f, 0.001f);
                const glm::vec2 uni = startSize * f;
                const glm::vec2 sc((startMinX + startMaxX) * 0.5f, (startMinY + startMaxY) * 0.5f);
                switch (m_dragScaleHandle) {
                    case 0: maxX = startMaxX; minX = startMaxX - uni.x; maxY = startMaxY; minY = startMaxY - uni.y; break;
                    case 1: minX = startMinX; maxX = startMinX + uni.x; maxY = startMaxY; minY = startMaxY - uni.y; break;
                    case 2: minX = startMinX; maxX = startMinX + uni.x; minY = startMinY; maxY = startMinY + uni.y; break;
                    case 3: maxX = startMaxX; minX = startMaxX - uni.x; minY = startMinY; maxY = startMinY + uni.y; break;
                    case 4: maxY = startMaxY; minY = startMaxY - uni.y; minX = sc.x - uni.x * 0.5f; maxX = sc.x + uni.x * 0.5f; break;
                    case 5: minX = startMinX; maxX = startMinX + uni.x; minY = sc.y - uni.y * 0.5f; maxY = sc.y + uni.y * 0.5f; break;
                    case 6: minY = startMinY; maxY = startMinY + uni.y; minX = sc.x - uni.x * 0.5f; maxX = sc.x + uni.x * 0.5f; break;
                    case 7: maxX = startMaxX; minX = startMaxX - uni.x; minY = sc.y - uni.y * 0.5f; maxY = sc.y + uni.y * 0.5f; break;
                }
            }

            const glm::vec2 newSize(maxX - minX, maxY - minY);
            // Below this the sprite is invisible and the scale sign would flip on
            // the next frame, which reads as the box snapping inside out.
            if (newSize.x > 0.01f && newSize.y > 0.01f) {
                const glm::vec2 newScale = newSize / spriteSize;
                transform->SetScale(newScale);

                // Resizing keeps the opposite edge anchored, so the quad's centre
                // moves. Its centre is pinned to the transform by the pivot
                // (newScale * quadCenter), so the transform itself has to shift by
                // the difference for the anchored edge to actually stay put.
                const glm::vec2 wantCenter((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
                const glm::vec2 deltaCenter = wantCenter - newScale * quadCenter;
                const float r = glm::radians(transform->localRotation);
                const float c = std::cos(r), s = std::sin(r);
                transform->SetPosition(m_scaleDragStartLocalPos +
                    glm::vec2(deltaCenter.x * c - deltaCenter.y * s,
                              deltaCenter.x * s + deltaCenter.y * c));
            }
        }
    }

    const bool draggingThis = (m_dragScaleHandle >= 0 && m_dragScaleId == transform->GetID());
    for (int i = 0; i < 8; ++i) {
        const bool isHot = (i == hovered) || (draggingThis && i == m_dragScaleHandle);
        const ImU32 color = isHot ? hotColor : handleColor;
        if (i < 4)
            drawList->AddRectFilled(ImVec2(handles[i].x - kBoxHandlePx, handles[i].y - kBoxHandlePx),
                                    ImVec2(handles[i].x + kBoxHandlePx, handles[i].y + kBoxHandlePx), color);
        else
            drawList->AddCircleFilled(handles[i], kBoxHandlePx, color);
    }
}

void GizmosManager::CommitSpriteBoxDrag()
{
    Container* container = Engine::Get()->GetActiveContainer();
    Registry* registry = container ? container->FindSystem<Registry>() : nullptr;
    if (!registry) return;

    auto* transform = registry->Find<Transform>(m_dragScaleId);
    if (!transform) return;

    // One gesture writes both, and MacroCommand::Wrap collapses them into a
    // single Ctrl+Z. Both property names already have branches in
    // GizmoUndoBridge, so nothing new is needed there.
    std::vector<GizmoEdit> edits;
    if (transform->localScale != m_scaleDragStartScale)
        edits.push_back({ m_dragScaleId, "localScale", m_scaleDragStartScale, transform->localScale });
    if (transform->localPosition != m_scaleDragStartLocalPos)
        edits.push_back({ m_dragScaleId, "localPosition", m_scaleDragStartLocalPos, transform->localPosition });

    if (!edits.empty()) Notify(EDIT_COMMITTED_EVENT, edits);
}

void GizmosManager::DrawTransformGizmo(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight) {
    Container* container = Engine::Get()->GetActiveContainer();
    SelectionManager* selectionManager = container->FindSystem<SelectionManager>();

    // Roots only: a selected child already follows its selected parent (MarkDirty
    // propagates), so applying the group delta to both would move it twice.
    std::vector<Transform*> targets;
    for (const std::string& id : selectionManager->GetSelectedRoots()) {
        auto* obj = dynamic_cast<GameObject*>(selectionManager->GetSerializable(id));
        if (!obj) continue;
        if (Transform* t = obj->GetComponent<Transform>()) targets.push_back(t);
    }
    if (targets.empty()) return;

    const bool multi = targets.size() > 1;

    // Pivot. One object: its own world matrix — which is already what we wrote last
    // frame, so ImGuizmo sees its own accumulated output fed back.
    //
    // Several: a translation-only matrix at the centroid of their world positions.
    // Only rebuilt when NOT dragging; during a drag ImGuizmo owns m_pivotMatrix and
    // accumulates into it. Recomputing it mid-drag would discard that accumulation
    // (see the member's comment).
    if (multi) {
        if (!m_transformGizmoActive) {
            glm::vec2 centroid(0.0f);
            for (Transform* t : targets) centroid += t->GetWorldPosition();
            centroid /= static_cast<float>(targets.size());
            m_pivotMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(centroid, 0.0f));
        }
    } else {
        m_pivotMatrix = targets.front()->GetWorldMatrix();
    }
    glm::mat4& pivotMatrix = m_pivotMatrix;

    ImGuizmo::SetOrthographic(true);
    ImGuizmo::SetRect(0, 0, viewWidth, viewHeight);

    ImGuizmo::OPERATION op;
    if (m_currentOperation == ImGuizmo::TRANSLATE) {
        op = static_cast<ImGuizmo::OPERATION>(ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y);
    } else if (m_currentOperation == ImGuizmo::SCALE) {
        op = static_cast<ImGuizmo::OPERATION>(ImGuizmo::SCALE_X | ImGuizmo::SCALE_Y);
    } else if (m_currentOperation == ImGuizmo::ROTATE) {
        op = ImGuizmo::ROTATE_Z;
    } else if (m_currentOperation == ImGuizmo::UNIVERSAL) {
        op = static_cast<ImGuizmo::OPERATION>(
            ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y |
            ImGuizmo::ROTATE_Z |
            ImGuizmo::SCALE_X | ImGuizmo::SCALE_Y
        );
    } else {
        return;
    }

    // Snapping is applied AFTER Manipulate rather than via ImGuizmo's snap arg: a
    // single Manipulate() call takes one snap array but each op reads it with
    // different units/meaning, and IsOver() can't disambiguate mid-drag. Manipulate
    // runs free; then the one component this frame changed is snapped below.

    // Capture the pre-drag state *before* Manipulate(): it mutates pivotMatrix on
    // the very frame the drag starts, so by the time IsUsing() is true the original
    // is already lost.
    const bool wasActive = m_transformGizmoActive;
    if (!wasActive) {
        m_dragRecords.clear();
        m_dragRecords.reserve(targets.size());
        for (Transform* t : targets)
            m_dragRecords.push_back({t->GetID(),
                                     t->localPosition, t->localRotation, t->localScale,
                                     t->GetWorldPosition(), t->GetWorldRotation(),
                                     t->GetWorldScale()});
        m_dragStartPivotWorld = pivotMatrix;
        m_dragRawRotDelta = 0.0f;
        m_dragAxis = 0;
    }

    // The pivot's rotation *before* Manipulate -- for the single-object rotation
    // accumulator (ImGuizmo adds only the per-frame delta onto this).
    float pivotRotBefore = 0.0f;
    {
        float t0[3], r0[3], s0[3];
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(pivotMatrix), t0, r0, s0);
        pivotRotBefore = r0[2];
    }

    // deltaMatrix is captured only to detect which operation a snap drag is
    // exercising (its per-frame translation/rotation/scale); it is NOT used to
    // apply the transform (see the note below on why it can't be).
    glm::mat4 deltaMatrix(1.0f);

    ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(proj),
        op,
        // A centroid pivot has no rotation basis, so LOCAL would silently degenerate
        // to WORLD for a multi-selection. Say so explicitly.
        multi ? ImGuizmo::WORLD : ImGuizmo::LOCAL,
        glm::value_ptr(pivotMatrix),
        // deltaMatrix stays null: ImGuizmo writes it in the pivot's LOCAL frame for
        // rotation (mModelInverse * delta * mModel) and as a bare origin-centered
        // scale, so it cannot be pre-multiplied onto another object's world matrix.
        // The group delta below is derived from drag-start matrices instead --
        // deltaMatrix here is read only for op detection, never applied.
        glm::value_ptr(deltaMatrix),
        nullptr);

    if (ImGuizmo::IsUsing()) {
        m_transformGizmoActive = true;

        if (!multi) {
            if ((m_snapRequested || m_uniformScale) && !m_dragRecords.empty()) {
                // Free-manipulation result, then adjust whichever component this drag
                // exercises: snap (Ctrl) and/or force uniform scale (Alt). The active
                // op is latched from the per-frame delta matrix (one handle drives one
                // op), and rotation uses a raw accumulator because ImGuizmo rotates the
                // reset pivot incrementally.
                const TransformDragRecord& rec = m_dragRecords.front();
                float cT[3], cR[3], cS[3];
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(pivotMatrix), cT, cR, cS);
                glm::vec2 pos(cT[0], cT[1]);
                float     rot = cR[2];
                glm::vec2 scl(cS[0], cS[1]);

                float perFrameRot = rot - pivotRotBefore;
                if (perFrameRot >  180.0f) perFrameRot -= 360.0f;
                if (perFrameRot < -180.0f) perFrameRot += 360.0f;
                m_dragRawRotDelta += perFrameRot;

                if (m_dragAxis == 0) {
                    float dT[3], dR[3], dS[3];
                    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(deltaMatrix), dT, dR, dS);
                    if      (std::abs(dS[0]-1.f) > 1e-3f || std::abs(dS[1]-1.f) > 1e-3f) m_dragAxis = 3;
                    else if (std::abs(dR[2]) > 1e-2f)                                    m_dragAxis = 2;
                    else if (glm::length(glm::vec2(dT[0], dT[1])) > 1e-2f)               m_dragAxis = 1;
                }

                if (m_dragAxis == 1) {           // translate
                    if (m_snapRequested) pos = SnapTo(pos, MoveSnapStep());
                } else if (m_dragAxis == 2) {    // rotate (Alt has no effect)
                    if (m_snapRequested) rot = SnapTo(rec.startWorldRot + m_dragRawRotDelta, RotateSnapDeg());
                } else if (m_dragAxis == 3) {    // scale
                    // Work in the ratio vs. drag start, not the absolute scale.
                    glm::vec2 safeStart = rec.startWorldScale;
                    if (std::abs(safeStart.x) < 1e-4f) safeStart.x = (safeStart.x < 0 ? -1e-4f : 1e-4f);
                    if (std::abs(safeStart.y) < 1e-4f) safeStart.y = (safeStart.y < 0 ? -1e-4f : 1e-4f);
                    glm::vec2 ratio = scl / safeStart;   // keeps sign

                    if (m_uniformScale) {
                        // Drive both axes by the one the user is actually changing.
                        const float f = (std::abs(ratio.x - 1.f) >= std::abs(ratio.y - 1.f))
                                            ? ratio.x : ratio.y;
                        ratio = glm::vec2(f, f);
                    }
                    if (m_snapRequested) {
                        // Snap the ratio, never below one step so it can't collapse.
                        ratio.x = std::max(SnapTo(std::abs(ratio.x), ScaleSnapStep()), ScaleSnapStep()) * (ratio.x < 0 ? -1.f : 1.f);
                        ratio.y = std::max(SnapTo(std::abs(ratio.y), ScaleSnapStep()), ScaleSnapStep()) * (ratio.y < 0 ? -1.f : 1.f);
                    }
                    scl = rec.startWorldScale * ratio;
                }

                Transform* t = targets.front();
                t->SetWorldPosition(pos);
                t->SetWorldRotation(rot);
                t->SetWorldScale(scl);
            } else {
                // Unchanged single-object path (no snap).
                ApplyWorld(targets.front(), pivotMatrix);
            }
        } else if (m_dragRecords.size() == targets.size()) {
            // Apply the gesture per-property rather than as one matrix product.
            //
            // `delta * startWorld` would be the conventional group transform, but it
            // makes rotation ORBIT each object around the centroid and scale push
            // them apart from it. What's wanted here is: translation moves the group
            // together, while rotation and scale act on each object IN PLACE.
            //
            // The decomposition is exact because the multi-select pivot is built
            // translation-only above — identity rotation, unit scale — so whatever
            // ImGuizmo wrote into it *is* the delta.
            float pivotT[3], pivotR[3], pivotS[3];
            ImGuizmo::DecomposeMatrixToComponents(
                glm::value_ptr(pivotMatrix), pivotT, pivotR, pivotS);

            const glm::vec2 startCentroid(m_dragStartPivotWorld[3]);
            glm::vec2 deltaPos   = glm::vec2(pivotT[0], pivotT[1]) - startCentroid;
            float     deltaRot   = pivotR[2];                       // start was 0
            glm::vec2 deltaScale = glm::vec2(pivotS[0], pivotS[1]); // start was 1

            if (m_snapRequested || m_uniformScale) {
                // Adjust the one active op's delta (see single-object path).
                if (glm::length(deltaPos) > kPosActiveEps) {
                    if (m_snapRequested)
                        deltaPos = SnapTo(startCentroid + deltaPos, MoveSnapStep()) - startCentroid;
                } else if (std::abs(deltaRot) > kRotActiveEps) {
                    if (m_snapRequested) deltaRot = SnapTo(deltaRot, RotateSnapDeg());
                } else {
                    if (m_uniformScale) {
                        const float f = (std::abs(deltaScale.x - 1.f) >= std::abs(deltaScale.y - 1.f))
                                            ? deltaScale.x : deltaScale.y;
                        deltaScale = glm::vec2(f, f);
                    }
                    if (m_snapRequested) {
                        deltaScale.x = std::max(SnapTo(deltaScale.x, ScaleSnapStep()), ScaleSnapStep());
                        deltaScale.y = std::max(SnapTo(deltaScale.y, ScaleSnapStep()), ScaleSnapStep());
                    }
                }
            }

            for (std::size_t i = 0; i < targets.size(); ++i) {
                const TransformDragRecord& record = m_dragRecords[i];
                targets[i]->SetWorldPosition(record.startWorldPos + deltaPos);
                targets[i]->SetWorldRotation(record.startWorldRot + deltaRot);
                targets[i]->SetWorldScale(record.startWorldScale * deltaScale);
            }
        }
    } else if (wasActive) {
        // Release: one event for the whole gesture.
        m_transformGizmoActive = false;
        CommitTransformDrag();
    }
}

void GizmosManager::ApplyWorld(Transform* transform, const glm::mat4& world)
{
    if (!transform) return;

    float matrixTranslation[3], matrixRotation[3], matrixScale[3];
    ImGuizmo::DecomposeMatrixToComponents(
        glm::value_ptr(world), matrixTranslation, matrixRotation, matrixScale);

    transform->SetWorldPosition(glm::vec2(matrixTranslation[0], matrixTranslation[1]));
    transform->SetWorldRotation(matrixRotation[2]);
    transform->SetWorldScale(glm::vec2(matrixScale[0], matrixScale[1]));
}

void GizmosManager::CommitTransformDrag()
{
    Container* container = Engine::Get()->GetActiveContainer();
    Registry* registry = container ? container->FindSystem<Registry>() : nullptr;
    if (!registry) { m_dragRecords.clear(); return; }

    // The gizmo writes all three every frame even for a translate-only op, so diff
    // rather than trusting the operation mode — otherwise a move would record a
    // no-op rotation and scale alongside it. With a centroid pivot a rotation
    // legitimately changes position too, which the diff picks up for free.
    std::vector<GizmoEdit> edits;
    for (const TransformDragRecord& record : m_dragRecords) {
        // Re-resolve by id rather than trusting a cached pointer: an object can be
        // destroyed mid-drag.
        Transform* transform = registry->Find<Transform>(record.transformId);
        if (!transform) continue;

        if (transform->localPosition != record.startLocalPos)
            edits.push_back({record.transformId, "localPosition",
                             record.startLocalPos, transform->localPosition});
        if (transform->localRotation != record.startLocalRot)
            edits.push_back({record.transformId, "localRotation",
                             record.startLocalRot, transform->localRotation});
        if (transform->localScale != record.startLocalScale)
            edits.push_back({record.transformId, "localScale",
                             record.startLocalScale, transform->localScale});
    }

    m_dragRecords.clear();
    // One notify for the whole gesture; the editor wraps whatever arrives into a
    // single undo entry.
    if (!edits.empty()) Notify(EDIT_COMMITTED_EVENT, edits);
}

void GizmosManager::CommitColliderDrag()
{
    Container* container = Engine::Get()->GetActiveContainer();
    Registry* registry = container ? container->FindSystem<Registry>() : nullptr;
    if (!registry) { m_dragColliderId.clear(); return; }

    std::vector<GizmoEdit> edits;
    const std::string id = m_dragColliderId;

    // Center lives on the Collider base, so it is diffed once for every type.
    if (auto* collider = registry->Find<Collider>(id)) {
        if (collider->GetCenter() != m_dragStartCenter)
            edits.push_back({id, "center", m_dragStartCenter, collider->GetCenter()});
    }

    if (auto* box = registry->Find<BoxCollider>(id)) {
        if (box->GetSize() != m_dragStartSize)
            edits.push_back({id, "size", m_dragStartSize, box->GetSize()});
    } else if (auto* circle = registry->Find<CircleCollider>(id)) {
        if (circle->GetRadius() != m_dragStartRadius)
            edits.push_back({id, "radius", m_dragStartRadius, circle->GetRadius()});
    } else if (auto* capsule = registry->Find<CapsuleCollider>(id)) {
        if (capsule->GetRadius() != m_dragStartRadius)
            edits.push_back({id, "radius", m_dragStartRadius, capsule->GetRadius()});
        if (capsule->GetHeight() != m_dragStartHeight)
            edits.push_back({id, "height", m_dragStartHeight, capsule->GetHeight()});
    }

    if (!edits.empty()) Notify(EDIT_COMMITTED_EVENT, edits);
}

void GizmosManager::CommitCameraDrag()
{
    Container* container = Engine::Get()->GetActiveContainer();
    Registry* registry = container ? container->FindSystem<Registry>() : nullptr;
    if (!registry) return;

    auto* camera = registry->Find<Camera>(m_dragCameraId);
    if (!camera) return;
    if (camera->GetOrthoSize() == m_dragStartOrthoSize) return;

    std::vector<GizmoEdit> edits{
        {m_dragCameraId, "orthoSize", m_dragStartOrthoSize, camera->GetOrthoSize()}
    };
    Notify(EDIT_COMMITTED_EVENT, edits);
}

void GizmosManager::DrawColliderGizmo(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight) {
    Container* container = Engine::Get()->GetActiveContainer();
    SelectionManager* selectionManager = container->FindSystem<SelectionManager>();
    GameObject* selectedObj = dynamic_cast<GameObject*>(selectionManager->GetSerializable());
    if (!selectedObj) return;
    Transform* transform = selectedObj->GetComponent<Transform>();

    // End any active drag as soon as the mouse is released. Centralised here so the
    // per-collider handlers below only start/process drags — with several colliders
    // each running its own hit-test, scattered release logic would race.
    ImGuiIO& io = ImGui::GetIO();
    if (!io.MouseDown[0]) {
        // Emit the finished gesture before clearing the state it is diffed against.
        if (m_dragHandle >= 0 && !m_dragColliderId.empty()) CommitColliderDrag();
        m_dragHandle = -1;
        m_dragColliderId.clear();
    }

    // Draw a gizmo for every collider on the object (of every type), so multiple
    // colliders are all visible and independently editable. Drag ownership is
    // scoped by collider id inside each Draw*ColliderGizmo.
    bool anyCollider = false;
    for (BoxCollider* boxCollider : selectedObj->GetComponents<BoxCollider>()) {
        anyCollider = true;
        DrawBoxColliderGizmo(view, proj, viewWidth, viewHeight, transform, boxCollider);
    }
    for (CircleCollider* circleCollider : selectedObj->GetComponents<CircleCollider>()) {
        anyCollider = true;
        DrawCircleColliderGizmo(view, proj, viewWidth, viewHeight, transform, circleCollider);
    }
    for (CapsuleCollider* capsuleCollider : selectedObj->GetComponents<CapsuleCollider>()) {
        anyCollider = true;
        DrawCapsuleColliderGizmo(view, proj, viewWidth, viewHeight, transform, capsuleCollider);
    }

    if (!anyCollider) DrawTransformGizmo(view, proj, viewWidth, viewHeight);
}

// ─── Camera Gizmo ───────────────────────────────────────────────────────────
// Orange rect showing the world region each enabled camera renders to the Game
// view. Scans every active GameObject (like Camera::GetMain) so the main
// camera's region is always visible without selecting it.
// ─── Joint gizmos ───────────────────────────────────────────────────────────
//
// A joint's anchors live in each body's LOCAL frame (pixels, measured from the
// body origin), so the world position is the body's world position plus the
// anchor rotated by the body's world rotation. Scale is deliberately left out:
// Box2D never scales a joint frame, so applying it here would draw the anchor
// somewhere the solver isn't using.
namespace {
    glm::vec2 JointAnchorToWorld(Transform* bodyTransform, const glm::vec2& localAnchor) {
        if (!bodyTransform) return localAnchor;
        const glm::vec2 origin = bodyTransform->GetWorldPosition();
        const float rot = glm::radians(bodyTransform->GetWorldRotation());
        const float c = std::cos(rot);
        const float s = std::sin(rot);
        return origin + glm::vec2(localAnchor.x * c - localAnchor.y * s,
                                  localAnchor.x * s + localAnchor.y * c);
    }
}

void GizmosManager::DrawJointGizmos(const glm::mat4& view, const glm::mat4& proj,
                                    float viewWidth, float viewHeight) {
    Container* container = Engine::Get()->GetActiveContainer();
    if (!container) return;
    Registry* registry = container->FindSystem<Registry>();
    SelectionManager* selectionManager = container->FindSystem<SelectionManager>();
    if (!registry || !selectionManager) return;

    const glm::mat4 vp = proj * view;
    for (const std::string& id : selectionManager->GetSelectedIds()) {
        GameObject* obj = registry->Find<GameObject>(id);
        if (!obj || !obj->GetActive()) continue;
        for (Joint* joint : obj->GetComponents<Joint>()) {
            if (!joint->GetEnabled()) continue;
            DrawJointGizmo(vp, viewWidth, viewHeight, joint);
        }
    }
}

void GizmosManager::DrawJointGizmo(const glm::mat4& vp, float viewWidth, float viewHeight,
                                   Joint* joint) {
    RigidBody* bodyA = joint->GetOwnRigidBody();
    RigidBody* bodyB = joint->GetConnectedRigidBody();
    if (!bodyA) return;

    Transform* transformA = bodyA->GetTransform();
    Transform* transformB = bodyB ? bodyB->GetTransform() : nullptr;
    if (!transformA) return;

    const glm::vec2 worldA = JointAnchorToWorld(transformA, joint->GetLocalAnchorA());
    const ImVec2 screenA = WorldToScreen(worldA, vp, viewWidth, viewHeight);

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    constexpr ImU32 kAnchorColor = IM_COL32(120, 220, 255, 255);
    constexpr ImU32 kLinkColor   = IM_COL32(120, 220, 255, 160);
    constexpr ImU32 kDetailColor = IM_COL32(255, 200,  90, 220);
    constexpr float kAnchorRadius = 5.0f;

    drawList->AddCircleFilled(screenA, kAnchorRadius, kAnchorColor);

    // An unconnected joint still shows its own anchor -- that is the state you are
    // most likely staring at while wiring one up.
    if (transformB) {
        const glm::vec2 worldB = JointAnchorToWorld(transformB, joint->GetLocalAnchorB());
        const ImVec2 screenB = WorldToScreen(worldB, vp, viewWidth, viewHeight);
        drawList->AddLine(screenA, screenB, kLinkColor, 2.0f);
        drawList->AddCircleFilled(screenB, kAnchorRadius, kAnchorColor);
        // Ring on B so the two ends are distinguishable when they overlap.
        drawList->AddCircle(screenB, kAnchorRadius + 3.0f, kAnchorColor, 0, 1.5f);
    }

    // Type-specific hint at anchor A: a ring for the hinge, the travel axis for
    // the sliding joints.
    if (dynamic_cast<RevoluteJoint*>(joint)) {
        drawList->AddCircle(screenA, kAnchorRadius + 6.0f, kDetailColor, 0, 2.0f);
    } else {
        float axisDeg = 0.0f;
        bool hasAxis = false;
        if (auto* prismatic = dynamic_cast<PrismaticJoint*>(joint)) {
            axisDeg = prismatic->GetAxisAngle(); hasAxis = true;
        } else if (auto* wheel = dynamic_cast<WheelJoint*>(joint)) {
            axisDeg = wheel->GetAxisAngle(); hasAxis = true;
        }
        if (hasAxis) {
            // The axis is authored relative to body A, so add the body's own rotation.
            const float rot = glm::radians(transformA->GetWorldRotation() + axisDeg);
            const glm::vec2 dir = {std::cos(rot), std::sin(rot)};
            constexpr float kAxisHalfLength = 28.0f;   // world units (pixels)
            const ImVec2 tip  = WorldToScreen(worldA + dir * kAxisHalfLength, vp, viewWidth, viewHeight);
            const ImVec2 tail = WorldToScreen(worldA - dir * kAxisHalfLength, vp, viewWidth, viewHeight);
            drawList->AddLine(tail, tip, kDetailColor, 2.0f);
        }
    }
}

// ─── Selection outline ───────────────────────────────────────────────────────
void GizmosManager::DrawSelectionOutlines(const glm::mat4& view, const glm::mat4& proj,
                                          float viewWidth, float viewHeight) {
    Container* container = Engine::Get()->GetActiveContainer();
    if (!container) return;
    Registry* registry = container->FindSystem<Registry>();
    SelectionManager* selectionManager = container->FindSystem<SelectionManager>();
    if (!registry || !selectionManager) return;

    const glm::mat4 vp = proj * view;
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    constexpr ImU32 kOutlineColor = IM_COL32(255, 255, 255, 235);
    constexpr float kOutlineThickness = 1.5f;

    for (const std::string& id : selectionManager->GetSelectedIds()) {
        GameObject* obj = registry->Find<GameObject>(id);
        if (!obj || !obj->GetActive()) continue;

        // Only sprites have a visual body to outline -- a bare Transform/Camera/
        // Light already gets its own gizmo above and no ambiguous "bounds" here.
        Transform* transform = obj->GetComponent<Transform>();
        SpriteRenderer* renderer = obj->GetComponent<SpriteRenderer>();
        if (!transform || !renderer) continue;

        Sprite* sprite = renderer->GetSprite();
        if (!sprite) continue;

        const glm::vec2 spriteSize = EngineUtils::RenderUtils::PixelsToWorld(sprite->GetPixelSize());
        if (spriteSize.x <= 0.0f || spriteSize.y <= 0.0f) continue;

        // Same quad-in-model-space math as DrawSpriteBoxGizmo / sprite.glsl's
        // vertex stage: centred at -uSize * pivot with half-extent uSize/2.
        const glm::vec2 quadHalf   = spriteSize * 0.5f;
        const glm::vec2 quadCenter = -spriteSize * sprite->GetPivot();
        const glm::mat4 worldMat = transform->GetWorldMatrix();
        const glm::vec2 localCorners[4] = {
            quadCenter + glm::vec2(-quadHalf.x, -quadHalf.y),
            quadCenter + glm::vec2( quadHalf.x, -quadHalf.y),
            quadCenter + glm::vec2( quadHalf.x,  quadHalf.y),
            quadCenter + glm::vec2(-quadHalf.x,  quadHalf.y),
        };

        ImVec2 screenCorners[4];
        for (int i = 0; i < 4; ++i)
            screenCorners[i] = WorldToScreen(
                glm::vec2(worldMat * glm::vec4(localCorners[i], 0.0f, 1.0f)), vp, viewWidth, viewHeight);

        for (int i = 0; i < 4; ++i)
            drawList->AddLine(screenCorners[i], screenCorners[(i + 1) % 4], kOutlineColor, kOutlineThickness);
    }
}

void GizmosManager::DrawCameraGizmos(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight) {
    Container* container = Engine::Get()->GetActiveContainer();
    if (!container) return;
    SceneManager* sceneManager = container->FindSystem<SceneManager>();
    if (!sceneManager) return;

    // End any active camera-corner drag the moment the mouse is released.
    // Centralised here (this runs every frame, before the per-camera draw loop)
    // so DrawCameraGizmo only has to start and process an ongoing drag.
    if (!ImGui::GetIO().MouseDown[0]) {
        if (m_dragCameraCorner >= 0 && !m_dragCameraId.empty()) CommitCameraDrag();
        m_dragCameraCorner = -1;
        m_dragCameraId.clear();
    }

    // Alpha tiers: a selected camera (its object is in the current selection) is
    // fully opaque; the active/main camera (the one driving the Game view) is
    // 75%; any other enabled camera is 50%. Selected wins over active.
    SelectionManager* selectionManager = container->FindSystem<SelectionManager>();
    Camera* mainCamera = Camera::GetMain();

    // Rect width follows the aspect the Game view ACTUALLY displays (it fills
    // its panel in Free mode, ignoring the camera's authored targetAspect), so
    // read the game view's resolved aspect. Fall back to 16:9 until it has
    // rendered a frame (targetW/H still 0) or if no Game view exists.
    float gameAspect = 16.0f / 9.0f;
    if (GameRenderView* gameView = Renderer::Get().GetGameView()) {
        float a = gameView->GetResolvedAspect();
        if (a > 0.0f) gameAspect = a;
    }

    constexpr int kAlphaBase     = 0;   // 50%
    constexpr int kAlphaActive   = 64;   // 75%
    constexpr int kAlphaSelected = 255;   // 100%

    for (Scene* scene : sceneManager->GetScenes()) {
        if (!scene) continue;
        for (GameObject* obj : scene->GetAllGameObjects()) {
            if (!obj || !obj->GetActive()) continue;
            Transform* transform = obj->GetComponent<Transform>();
            if (!transform) continue;
            for (Camera* camera : obj->GetComponents<Camera>()) {
                if (!camera->GetEnabled()) continue;
                int alpha = kAlphaBase;
                if (selectionManager && selectionManager->IsSelected(obj->GetID()))
                    alpha = kAlphaSelected;
                else if (camera == mainCamera)         alpha = kAlphaActive;
                DrawCameraGizmo(view, proj, viewWidth, viewHeight, transform, camera, alpha, gameAspect);
            }
        }
    }
}

void GizmosManager::DrawCameraGizmo(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight,
                                    Transform* transform, Camera* camera, int alpha, float aspect) {
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // World-space half-extents of the camera's view rect. halfHeight == orthoSize
    // (a game camera never zooms -- see Camera::ApplyTo), so the rect scales
    // proportionally with orthoSize. halfWidth applies the Game view's displayed
    // aspect (passed in), not the camera's authored targetAspect.
    float halfHeight = camera->GetOrthoSize();
    float halfWidth  = halfHeight * aspect;

    // The camera renders from its object's world position + rotation only
    // (Camera::ApplyTo ignores scale), so build the rect the same way instead
    // of using the full world matrix.
    glm::vec2 center = transform->GetWorldPosition();
    float rot = glm::radians(transform->GetWorldRotation());
    float cosR = std::cos(rot), sinR = std::sin(rot);

    glm::vec2 localCorners[4] = {
        { -halfWidth, -halfHeight },  // bottom-left
        {  halfWidth, -halfHeight },  // bottom-right
        {  halfWidth,  halfHeight },  // top-right
        { -halfWidth,  halfHeight },  // top-left
    };

    glm::mat4 vp = proj * view;
    ImVec2 screenCorners[4];
    for (int i = 0; i < 4; i++) {
        glm::vec2 rotated = {
            localCorners[i].x * cosR - localCorners[i].y * sinR,
            localCorners[i].x * sinR + localCorners[i].y * cosR,
        };
        screenCorners[i] = WorldToScreen(center + rotated, vp, viewWidth, viewHeight);
    }

    ImU32 outlineColor = IM_COL32(255, 200, 0, alpha);   // orange; alpha by camera state
    float lineThickness = 2.0f;
    for (int i = 0; i < 4; i++) {
        drawList->AddLine(screenCorners[i], screenCorners[(i + 1) % 4], outlineColor, lineThickness);
    }

    // A rect drawn fully transparent (alpha 0 -- a non-selected, non-main
    // camera) isn't visible, so it gets no grab handles and no interaction.
    if (alpha <= 0) return;

    // ─── Interactive corners: drag one to rescale the camera's orthoSize ──────
    // The rect is fully determined by orthoSize (halfHeight == orthoSize,
    // halfWidth == orthoSize * aspect), so moving any corner just needs to solve
    // for a new orthoSize and every corner follows. We map the cursor into the
    // camera's local (un-rotated, center-relative) frame and project it onto the
    // dragged corner's diagonal direction (signX*aspect, signY): the projection
    // length is exactly the orthoSize that puts that corner under the cursor,
    // while keeping the aspect locked.
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;

    constexpr float kCornerHandleRadius = 5.0f;
    constexpr float kCornerHitRadius    = kCornerHandleRadius + 4.0f;
    constexpr float kMinOrthoSize       = 1.0f;

    // Hit-test this camera's corners (skip while another camera owns the drag).
    int hoveredCorner = -1;
    if (m_dragCameraCorner < 0) {
        for (int i = 0; i < 4; i++) {
            float dx = mousePos.x - screenCorners[i].x;
            float dy = mousePos.y - screenCorners[i].y;
            if (dx * dx + dy * dy < kCornerHitRadius * kCornerHitRadius) {
                hoveredCorner = i;
                break;
            }
        }
    }
    if (hoveredCorner >= 0) m_hoveredCameraCorner = hoveredCorner;

    // Start a drag: only if nothing else (ImGuizmo, a collider handle, or another
    // camera) already owns the mouse this frame.
    if (io.MouseClicked[0] && hoveredCorner >= 0 &&
        !ImGuizmo::IsUsing() && m_dragHandle < 0 && m_dragCameraCorner < 0) {
        m_dragCameraCorner   = hoveredCorner;
        m_dragCameraId       = camera->GetID();
        m_dragStartOrthoSize = camera->GetOrthoSize();
    }

    // Process the drag for the camera that owns it.
    if (m_dragCameraCorner >= 0 && m_dragCameraId == camera->GetID() && io.MouseDown[0]) {
        // corners: 0=BL(-,-) 1=BR(+,-) 2=TR(+,+) 3=TL(-,+)
        float signX = (m_dragCameraCorner == 1 || m_dragCameraCorner == 2) ? 1.0f : -1.0f;
        float signY = (m_dragCameraCorner == 2 || m_dragCameraCorner == 3) ? 1.0f : -1.0f;

        glm::vec2 mouseWorld = ScreenToWorld(mousePos, vp, viewWidth, viewHeight);
        glm::vec2 relWorld   = mouseWorld - center;
        // Rotate world delta into the camera's local frame: local = R(-rot) * rel.
        glm::vec2 localMouse(
             relWorld.x * cosR + relWorld.y * sinR,
            -relWorld.x * sinR + relWorld.y * cosR);

        glm::vec2 diag(signX * aspect, signY);
        float newOrthoSize = glm::dot(localMouse, diag) / glm::dot(diag, diag);
        camera->SetOrthoSize(std::max(newOrthoSize, kMinOrthoSize));
    }

    // Draw the corner handles (brighter when hovered or actively dragged).
    ImU32 handleColor      = IM_COL32(255, 200, 0, std::max(alpha, 160));
    ImU32 handleHoverColor = IM_COL32(255, 235, 150, 255);
    bool draggingThis = (m_dragCameraCorner >= 0 && m_dragCameraId == camera->GetID());
    for (int i = 0; i < 4; i++) {
        bool isHot = (i == hoveredCorner) || (draggingThis && i == m_dragCameraCorner);
        drawList->AddRectFilled(
            ImVec2(screenCorners[i].x - kCornerHandleRadius, screenCorners[i].y - kCornerHandleRadius),
            ImVec2(screenCorners[i].x + kCornerHandleRadius, screenCorners[i].y + kCornerHandleRadius),
            isHot ? handleHoverColor : handleColor);
    }
}

// ─── Light + ShadowCaster gizmos ────────────────────────────────────────────
//
// Structured exactly like the camera gizmos above: every enabled light in every
// loaded scene is drawn (dim when unselected, opaque when selected) so a
// lighting setup reads at a glance, and only the selected object's gizmo carries
// grab handles.
//
// All handle ids -- lights and shadow casters alike -- share one flat space, so
// a single (m_dragLightHandle, m_dragLightId) pair scopes every drag. The id
// half is what stops two lights from both claiming the same click.
namespace {
    enum LightHandle {
        // Point: four cardinal grips on the range ring.
        kPointRangeE = 0, kPointRangeN, kPointRangeW, kPointRangeS,
        kPointInnerRadius = 4,
        // Spot: one grip down the cone axis, one on each angle edge.
        kSpotRange = 5, kSpotInnerAngle = 6, kSpotOuterAngle = 7,
        // ShadowCaster: four box corners, or four cardinal grips on the circle.
        kCasterCorner0 = 8,   // .. 11
        kCasterRadius0 = 12,  // .. 15
    };

    constexpr float kHandleRadiusPx    = 4.5f;
    constexpr float kHandleHitRadiusPx = kHandleRadiusPx + 4.0f;
    // Inner-radius grip never sits closer than this to the light's centre, so it
    // stays grabbable (and clear of the transform gizmo) even at innerRadius 0.
    // The drawn inner ring still shows the true value -- only the grip is pushed out.
    constexpr float kMinInnerHandlePx  = 20.0f;
    constexpr int   kAlphaUnselected   = 70;
    constexpr int   kAlphaSelected     = 255;

    glm::vec2 RotateVec(const glm::vec2& v, float radians) {
        const float c = std::cos(radians), s = std::sin(radians);
        return { v.x * c - v.y * s, v.x * s + v.y * c };
    }

    float ScreenDistance(const ImVec2& a, const ImVec2& b) {
        const float dx = a.x - b.x, dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    bool HitTest(const ImVec2& mouse, const ImVec2& handle) {
        const float dx = mouse.x - handle.x, dy = mouse.y - handle.y;
        return dx * dx + dy * dy < kHandleHitRadiusPx * kHandleHitRadiusPx;
    }

    // Signed-magnitude angle in degrees between `dir` and `to`.
    float AngleBetweenDeg(const glm::vec2& dir, const glm::vec2& to) {
        const float len = glm::length(to);
        if (len < 1e-6f) return 0.0f;
        const float d = glm::clamp(glm::dot(dir, to / len), -1.0f, 1.0f);
        return std::acos(d) * EngineUtils::MathUtils::RAD_2_DEG;
    }
}

void GizmosManager::DrawLightGizmos(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight) {
    Container* container = Engine::Get()->GetActiveContainer();
    if (!container) return;
    SceneManager* sceneManager = container->FindSystem<SceneManager>();
    if (!sceneManager) return;

    // End any active light/caster drag the moment the mouse is released.
    // Centralised here, before the per-light loop, for the same reason the camera
    // and collider gizmos do it: several lights each run their own hit-test, and
    // scattered release logic would race between them.
    if (!ImGui::GetIO().MouseDown[0]) {
        if (m_dragLightHandle >= 0 && !m_dragLightId.empty()) CommitLightDrag();
        m_dragLightHandle = -1;
        m_dragLightId.clear();
    }

    SelectionManager* selectionManager = container->FindSystem<SelectionManager>();
    const glm::mat4 vp = proj * view;

    for (Scene* scene : sceneManager->GetScenes()) {
        if (!scene) continue;
        for (GameObject* obj : scene->GetAllGameObjects()) {
            if (!obj || !obj->GetActive()) continue;
            Transform* transform = obj->GetComponent<Transform>();
            if (!transform) continue;

            const bool selected = selectionManager && selectionManager->IsSelected(obj->GetID());
            const int  alpha    = selected ? kAlphaSelected : kAlphaUnselected;

            for (Light* light : obj->GetComponents<Light>()) {
                if (!light->GetEnabled()) continue;
                switch (light->GetType()) {
                    case Light::LightType::Point:
                        DrawPointLightGizmo(vp, viewWidth, viewHeight, transform, light, alpha, selected);
                        break;
                    case Light::LightType::Spot:
                        DrawSpotLightGizmo(vp, viewWidth, viewHeight, transform, light, alpha, selected);
                        break;
                    case Light::LightType::Directional:
                        DrawDirectionalLightGizmo(vp, viewWidth, viewHeight, transform, light, alpha);
                        break;
                    case Light::LightType::Global:
                        // A flat ambient fill has no position, direction or extent
                        // to draw. Its component icon is the whole gizmo.
                        break;
                }
            }

            for (ShadowCaster* caster : obj->GetComponents<ShadowCaster>()) {
                if (!caster->GetEnabled()) continue;
                DrawShadowCasterGizmo(vp, viewWidth, viewHeight, caster, alpha, selected);
            }
        }
    }
}

void GizmosManager::DrawPointLightGizmo(const glm::mat4& vp, float viewWidth, float viewHeight,
                                        Transform* transform, Light* light, int alpha, bool interactive) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // Range is authored in world units and LightingPass consumes it unscaled, so
    // the gizmo must ignore the transform's scale too -- same convention as the
    // camera rect, which follows orthoSize rather than scale.
    const glm::vec2 worldCenter = transform->GetWorldPosition();
    const float range = light->GetRange();

    const ImVec2 screenCenter = WorldToScreen(worldCenter, vp, viewWidth, viewHeight);
    const ImVec2 screenEdge   = WorldToScreen(worldCenter + glm::vec2(range, 0.0f), vp, viewWidth, viewHeight);
    const float  screenRadius = ScreenDistance(screenEdge, screenCenter);

    // Tinted with the light's own colour so several lights are told apart at a
    // glance; alpha carries the selection state.
    const glm::vec4 c = light->GetColor();
    const ImU32 ringColor = IM_COL32(int(c.r * 255), int(c.g * 255), int(c.b * 255), alpha);
    const ImU32 innerColor = IM_COL32(int(c.r * 255), int(c.g * 255), int(c.b * 255), alpha / 2);

    drawList->AddCircle(screenCenter, screenRadius, ringColor, 64, 1.5f);

    const float innerRadius = light->GetInnerRadius();
    if (innerRadius > 0.001f)
        drawList->AddCircle(screenCenter, screenRadius * innerRadius, innerColor, 48, 1.0f);

    if (!interactive) return;

    ImVec2 handles[5] = {
        ImVec2(screenCenter.x + screenRadius, screenCenter.y),   // E
        ImVec2(screenCenter.x, screenCenter.y - screenRadius),   // N
        ImVec2(screenCenter.x - screenRadius, screenCenter.y),   // W
        ImVec2(screenCenter.x, screenCenter.y + screenRadius),   // S
        ImVec2(0, 0),                                            // inner radius (below)
    };
    // Diagonal, so it can never sit under one of the cardinal range grips.
    const float innerPx = std::max(screenRadius * innerRadius, kMinInnerHandlePx);
    const float diag = 0.70710678f;
    handles[4] = ImVec2(screenCenter.x + innerPx * diag, screenCenter.y - innerPx * diag);

    const int ids[5] = { kPointRangeE, kPointRangeN, kPointRangeW, kPointRangeS, kPointInnerRadius };

    int hovered = -1;
    if (m_dragLightHandle < 0) {
        for (int i = 0; i < 5; ++i)
            if (HitTest(io.MousePos, handles[i])) { hovered = ids[i]; break; }
    }
    if (hovered >= 0) m_hoveredLightHandle = hovered;

    // Only claim the click if nothing else already owns the mouse this frame.
    if (io.MouseClicked[0] && hovered >= 0 && !ImGuizmo::IsUsing() &&
        m_dragHandle < 0 && m_dragCameraCorner < 0 && m_dragLightHandle < 0) {
        m_dragLightHandle      = hovered;
        m_dragLightId          = light->GetID();
        m_dragStartRange       = range;
        m_dragStartInnerRadius = innerRadius;
    }

    if (m_dragLightHandle >= 0 && m_dragLightId == light->GetID() && io.MouseDown[0]) {
        const glm::vec2 mouseWorld = ScreenToWorld(io.MousePos, vp, viewWidth, viewHeight);
        const float worldDist = glm::length(mouseWorld - worldCenter);
        if (m_dragLightHandle == kPointInnerRadius) {
            // Expressed as a fraction of range, which is how the shader reads it.
            light->SetInnerRadius(range > 1e-6f ? worldDist / range : 0.0f);
        } else if (m_dragLightHandle <= kPointRangeS) {
            light->SetRange(worldDist);
        }
    }

    const ImU32 handleColor = IM_COL32(255, 235, 150, 255);
    const ImU32 hotColor    = IM_COL32(255, 255, 255, 255);
    const bool draggingThis = (m_dragLightHandle >= 0 && m_dragLightId == light->GetID());
    for (int i = 0; i < 5; ++i) {
        const bool isHot = (ids[i] == hovered) || (draggingThis && ids[i] == m_dragLightHandle);
        drawList->AddCircleFilled(handles[i], kHandleRadiusPx, isHot ? hotColor : handleColor);
    }
}

void GizmosManager::DrawSpotLightGizmo(const glm::mat4& vp, float viewWidth, float viewHeight,
                                       Transform* transform, Light* light, int alpha, bool interactive) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    const glm::vec2 worldCenter = transform->GetWorldPosition();
    const float range = light->GetRange();
    // Direction is the Transform's world rotation, not an authored field -- the
    // rotate gizmo is what turns a spotlight.
    const glm::vec2 dir = light->GetWorldDirection();

    const float innerRad = light->GetInnerAngle() * EngineUtils::MathUtils::DEG_2_RAD;
    const float outerRad = light->GetOuterAngle() * EngineUtils::MathUtils::DEG_2_RAD;

    const ImVec2 screenCenter = WorldToScreen(worldCenter, vp, viewWidth, viewHeight);

    const glm::vec4 c = light->GetColor();
    const ImU32 coneColor  = IM_COL32(int(c.r * 255), int(c.g * 255), int(c.b * 255), alpha);
    const ImU32 innerColor = IM_COL32(int(c.r * 255), int(c.g * 255), int(c.b * 255), alpha / 2);

    // Arc + the two edge rays, for both the outer cone and the inner core.
    auto drawCone = [&](float halfAngle, float radiusScale, ImU32 color, float thickness) {
        const float r = range * radiusScale;
        const ImVec2 edgeA = WorldToScreen(worldCenter + RotateVec(dir, -halfAngle) * r, vp, viewWidth, viewHeight);
        const ImVec2 edgeB = WorldToScreen(worldCenter + RotateVec(dir,  halfAngle) * r, vp, viewWidth, viewHeight);
        drawList->AddLine(screenCenter, edgeA, color, thickness);
        drawList->AddLine(screenCenter, edgeB, color, thickness);

        constexpr int kArcSegments = 32;
        ImVec2 prev = edgeA;
        for (int i = 1; i <= kArcSegments; ++i) {
            const float t = static_cast<float>(i) / kArcSegments;
            const float a = -halfAngle + (2.0f * halfAngle) * t;
            const ImVec2 p = WorldToScreen(worldCenter + RotateVec(dir, a) * r, vp, viewWidth, viewHeight);
            drawList->AddLine(prev, p, color, thickness);
            prev = p;
        }
    };

    drawCone(outerRad, 1.0f, coneColor, 1.5f);
    if (light->GetInnerAngle() > 0.01f)
        drawCone(innerRad, 1.0f, innerColor, 1.0f);

    if (!interactive) return;

    // Inner-angle grip is pulled inward so it can't land on top of the
    // outer-angle grip when the two angles are equal.
    constexpr float kInnerHandleScale = 0.6f;
    const ImVec2 handles[3] = {
        WorldToScreen(worldCenter + dir * range, vp, viewWidth, viewHeight),
        WorldToScreen(worldCenter + RotateVec(dir, innerRad) * (range * kInnerHandleScale), vp, viewWidth, viewHeight),
        WorldToScreen(worldCenter + RotateVec(dir, outerRad) * range, vp, viewWidth, viewHeight),
    };
    const int ids[3] = { kSpotRange, kSpotInnerAngle, kSpotOuterAngle };

    int hovered = -1;
    if (m_dragLightHandle < 0) {
        for (int i = 0; i < 3; ++i)
            if (HitTest(io.MousePos, handles[i])) { hovered = ids[i]; break; }
    }
    if (hovered >= 0) m_hoveredLightHandle = hovered;

    if (io.MouseClicked[0] && hovered >= 0 && !ImGuizmo::IsUsing() &&
        m_dragHandle < 0 && m_dragCameraCorner < 0 && m_dragLightHandle < 0) {
        m_dragLightHandle     = hovered;
        m_dragLightId         = light->GetID();
        m_dragStartRange      = range;
        m_dragStartInnerAngle = light->GetInnerAngle();
        m_dragStartOuterAngle = light->GetOuterAngle();
    }

    if (m_dragLightHandle >= 0 && m_dragLightId == light->GetID() && io.MouseDown[0]) {
        const glm::vec2 mouseWorld = ScreenToWorld(io.MousePos, vp, viewWidth, viewHeight);
        const glm::vec2 toMouse = mouseWorld - worldCenter;
        if (m_dragLightHandle == kSpotRange) {
            light->SetRange(glm::length(toMouse));
        } else if (m_dragLightHandle == kSpotInnerAngle) {
            light->SetInnerAngle(AngleBetweenDeg(dir, toMouse));
        } else if (m_dragLightHandle == kSpotOuterAngle) {
            light->SetOuterAngle(AngleBetweenDeg(dir, toMouse));
        }
    }

    const ImU32 handleColor = IM_COL32(255, 235, 150, 255);
    const ImU32 hotColor    = IM_COL32(255, 255, 255, 255);
    const bool draggingThis = (m_dragLightHandle >= 0 && m_dragLightId == light->GetID());
    for (int i = 0; i < 3; ++i) {
        const bool isHot = (ids[i] == hovered) || (draggingThis && ids[i] == m_dragLightHandle);
        drawList->AddCircleFilled(handles[i], kHandleRadiusPx, isHot ? hotColor : handleColor);
    }
}

void GizmosManager::DrawDirectionalLightGizmo(const glm::mat4& vp, float viewWidth, float viewHeight,
                                              Transform* transform, Light* light, int alpha) {
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    const glm::vec2 worldCenter = transform->GetWorldPosition();
    const glm::vec2 dir = light->GetWorldDirection();
    const glm::vec2 perp(-dir.y, dir.x);

    // A directional light has no position or extent that means anything, so this
    // is purely an orientation readout: parallel rays showing which way the light
    // travels. Deliberately NON-interactive -- direction comes from the
    // Transform's rotation, and the rotate gizmo already edits that. A second set
    // of handles here would be a second source of truth.
    constexpr float kRayLength  = 160.0f;   // world units
    constexpr float kRaySpacing = 34.0f;
    constexpr int   kRayCount   = 3;

    const glm::vec4 c = light->GetColor();
    const ImU32 color = IM_COL32(int(c.r * 255), int(c.g * 255), int(c.b * 255), alpha);

    for (int i = -(kRayCount / 2); i <= kRayCount / 2; ++i) {
        const glm::vec2 offset = perp * (static_cast<float>(i) * kRaySpacing);
        const glm::vec2 start = worldCenter + offset - dir * (kRayLength * 0.5f);
        const glm::vec2 end   = worldCenter + offset + dir * (kRayLength * 0.5f);
        const ImVec2 s = WorldToScreen(start, vp, viewWidth, viewHeight);
        const ImVec2 e = WorldToScreen(end,   vp, viewWidth, viewHeight);
        drawList->AddLine(s, e, color, 1.5f);

        // Arrowhead: two short barbs swept back from the tip.
        const glm::vec2 barbA = end - RotateVec(dir, 0.4f) * 18.0f;
        const glm::vec2 barbB = end - RotateVec(dir, -0.4f) * 18.0f;
        drawList->AddLine(e, WorldToScreen(barbA, vp, viewWidth, viewHeight), color, 1.5f);
        drawList->AddLine(e, WorldToScreen(barbB, vp, viewWidth, viewHeight), color, 1.5f);
    }
}

void GizmosManager::DrawShadowCasterGizmo(const glm::mat4& vp, float viewWidth, float viewHeight,
                                          ShadowCaster* caster, int alpha, bool interactive) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    const ImU32 color = IM_COL32(150, 150, 255, alpha);
    const auto shape = caster->GetShape();

    // Drawn from the same code paths that feed the shadow atlas, so what is shown
    // here is exactly the silhouette that gets rasterized -- the two can't drift
    // apart. SpriteAlpha has no single closed loop (a sprite can have holes and
    // disjoint islands), so it draws its unordered segment list directly.
    if (shape == ShadowCaster::Shape::SpriteAlpha) {
        std::vector<glm::vec2> segments;
        caster->AppendSegments(segments);
        for (std::size_t i = 0; i + 1 < segments.size(); i += 2) {
            drawList->AddLine(WorldToScreen(segments[i],     vp, viewWidth, viewHeight),
                              WorldToScreen(segments[i + 1], vp, viewWidth, viewHeight),
                              color, 1.5f);
        }
        return;   // derived from the sprite's pixels: nothing here is draggable
    }

    std::vector<glm::vec2> outline;
    caster->BuildOutline(outline);
    if (outline.size() < 2) return;

    for (std::size_t i = 0; i < outline.size(); ++i) {
        const ImVec2 a = WorldToScreen(outline[i], vp, viewWidth, viewHeight);
        const ImVec2 b = WorldToScreen(outline[(i + 1) % outline.size()], vp, viewWidth, viewHeight);
        drawList->AddLine(a, b, color, 1.5f);
    }

    // SpriteBounds and FromCollider are DERIVED from another component, so they
    // get no handles -- editing them means editing the sprite or the collider.
    if (!interactive || (shape != ShadowCaster::Shape::Box && shape != ShadowCaster::Shape::Circle))
        return;

    GameObject* obj = caster->GetGameObject();
    Transform* transform = obj ? obj->GetTransform() : nullptr;
    if (!transform) return;
    const glm::mat4 worldMat = transform->GetWorldMatrix();
    const glm::mat4 invWorld = glm::inverse(worldMat);

    const glm::vec2 localCenter = caster->GetCenter();
    const glm::vec2 worldCenter = glm::vec2(worldMat * glm::vec4(localCenter, 0.0f, 1.0f));

    ImVec2 handles[4];
    int ids[4];
    if (shape == ShadowCaster::Shape::Box) {
        const glm::vec2 h = caster->GetSize() * 0.5f;
        const glm::vec2 corners[4] = {
            localCenter + glm::vec2(-h.x, -h.y), localCenter + glm::vec2(h.x, -h.y),
            localCenter + glm::vec2( h.x,  h.y), localCenter + glm::vec2(-h.x, h.y),
        };
        for (int i = 0; i < 4; ++i) {
            handles[i] = WorldToScreen(glm::vec2(worldMat * glm::vec4(corners[i], 0.0f, 1.0f)),
                                       vp, viewWidth, viewHeight);
            ids[i] = kCasterCorner0 + i;
        }
    } else {
        const float r = caster->GetRadius();
        const glm::vec2 dirs[4] = { {1,0}, {0,1}, {-1,0}, {0,-1} };
        for (int i = 0; i < 4; ++i) {
            handles[i] = WorldToScreen(glm::vec2(worldMat * glm::vec4(localCenter + dirs[i] * r, 0.0f, 1.0f)),
                                       vp, viewWidth, viewHeight);
            ids[i] = kCasterRadius0 + i;
        }
    }

    int hovered = -1;
    if (m_dragLightHandle < 0) {
        for (int i = 0; i < 4; ++i)
            if (HitTest(io.MousePos, handles[i])) { hovered = ids[i]; break; }
    }
    if (hovered >= 0) m_hoveredLightHandle = hovered;

    if (io.MouseClicked[0] && hovered >= 0 && !ImGuizmo::IsUsing() &&
        m_dragHandle < 0 && m_dragCameraCorner < 0 && m_dragLightHandle < 0) {
        m_dragLightHandle       = hovered;
        m_dragLightId           = caster->GetID();
        m_dragStartCasterSize   = caster->GetSize();
        m_dragStartCasterRadius = caster->GetRadius();
    }

    if (m_dragLightHandle >= 0 && m_dragLightId == caster->GetID() && io.MouseDown[0]) {
        const glm::vec2 mouseWorld = ScreenToWorld(io.MousePos, vp, viewWidth, viewHeight);
        // Solve in LOCAL space so the drag stays correct under a rotated or
        // non-uniformly scaled parent.
        const glm::vec2 localMouse = glm::vec2(invWorld * glm::vec4(mouseWorld, 0.0f, 1.0f));
        if (shape == ShadowCaster::Shape::Box) {
            // Symmetric about the caster's centre: dragging one corner moves all
            // four, which keeps `center` meaning "the middle of the box".
            const glm::vec2 half = glm::abs(localMouse - localCenter);
            caster->SetSize(half * 2.0f);
        } else {
            caster->SetRadius(glm::length(localMouse - localCenter));
        }
    }

    const ImU32 handleColor = IM_COL32(190, 190, 255, 255);
    const ImU32 hotColor    = IM_COL32(255, 255, 255, 255);
    const bool draggingThis = (m_dragLightHandle >= 0 && m_dragLightId == caster->GetID());
    for (int i = 0; i < 4; ++i) {
        const bool isHot = (ids[i] == hovered) || (draggingThis && ids[i] == m_dragLightHandle);
        drawList->AddCircleFilled(handles[i], kHandleRadiusPx, isHot ? hotColor : handleColor);
    }

    // Centre crosshair, matching the collider gizmos.
    const ImVec2 sc = WorldToScreen(worldCenter, vp, viewWidth, viewHeight);
    drawList->AddLine(ImVec2(sc.x - 5, sc.y), ImVec2(sc.x + 5, sc.y), color, 1.0f);
    drawList->AddLine(ImVec2(sc.x, sc.y - 5), ImVec2(sc.x, sc.y + 5), color, 1.0f);
}

void GizmosManager::CommitLightDrag()
{
    Container* container = Engine::Get()->GetActiveContainer();
    Registry* registry = container ? container->FindSystem<Registry>() : nullptr;
    if (!registry) return;

    std::vector<GizmoEdit> edits;
    const std::string& id = m_dragLightId;

    if (auto* light = registry->Find<Light>(id)) {
        if (light->GetRange() != m_dragStartRange)
            edits.push_back({ id, "range", m_dragStartRange, light->GetRange() });
        if (light->GetInnerRadius() != m_dragStartInnerRadius)
            edits.push_back({ id, "innerRadius", m_dragStartInnerRadius, light->GetInnerRadius() });
        // Inner and outer clamp against each other, so a drag on one can move the
        // other. Diffing both means the undo entry restores the pair.
        if (light->GetInnerAngle() != m_dragStartInnerAngle)
            edits.push_back({ id, "innerAngle", m_dragStartInnerAngle, light->GetInnerAngle() });
        if (light->GetOuterAngle() != m_dragStartOuterAngle)
            edits.push_back({ id, "outerAngle", m_dragStartOuterAngle, light->GetOuterAngle() });
    } else if (auto* caster = registry->Find<ShadowCaster>(id)) {
        if (caster->GetSize() != m_dragStartCasterSize)
            edits.push_back({ id, "casterSize", m_dragStartCasterSize, caster->GetSize() });
        if (caster->GetRadius() != m_dragStartCasterRadius)
            edits.push_back({ id, "casterRadius", m_dragStartCasterRadius, caster->GetRadius() });
    }

    if (!edits.empty()) Notify(EDIT_COMMITTED_EVENT, edits);
}

// ─── AudioSource gizmos ──────────────────────────────────────────────────────
// Audio attenuation rings are selected-only because their authored ranges are
// commonly much larger than lights and would otherwise obscure the viewport.
// The component icon remains visible on every active object.
namespace {
    enum AudioHandle {
        kAudioMinDistance = 0,
        kAudioMaxDistance = 1,
    };
}

void GizmosManager::DrawAudioSourceGizmos(const glm::mat4& view, const glm::mat4& proj,
                                          float viewWidth, float viewHeight) {
    Container* container = Engine::Get()->GetActiveContainer();
    if (!container) return;

    // Commit once on release, after all live drag updates have finished.
    if (!ImGui::GetIO().MouseDown[0]) {
        if (m_dragAudioHandle >= 0 && !m_dragAudioId.empty()) CommitAudioSourceDrag();
        m_dragAudioHandle = -1;
        m_dragAudioId.clear();
    }

    SceneManager* sceneManager = container->FindSystem<SceneManager>();
    SelectionManager* selectionManager = container->FindSystem<SelectionManager>();
    if (!sceneManager || !selectionManager || !selectionManager->HasSelection()) return;

    const glm::mat4 vp = proj * view;
    for (Scene* scene : sceneManager->GetScenes()) {
        if (!scene) continue;
        for (GameObject* obj : scene->GetAllGameObjects()) {
            if (!obj || !obj->GetActive() || !selectionManager->IsSelected(obj->GetID())) continue;

            Transform* transform = obj->GetComponent<Transform>();
            if (!transform) continue;

            for (AudioSource* source : obj->GetComponents<AudioSource>()) {
                if (source && source->GetEnabled())
                    DrawAudioSourceGizmo(vp, viewWidth, viewHeight, transform, source);
            }
        }
    }
}

void GizmosManager::DrawAudioSourceGizmo(const glm::mat4& vp, float viewWidth, float viewHeight,
                                         Transform* transform, AudioSource* source) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // miniaudio consumes these as world distances, independent of Transform
    // scale. Clamp only for drawing so legacy negative values cannot invert the
    // screen radius; the first handle drag repairs such a value.
    const glm::vec2 worldCenter = transform->GetWorldPosition();
    const float minDistance = std::max(0.0f, source->GetMinDistance());
    const float maxDistance = std::max(0.0f, source->GetMaxDistance());
    const ImVec2 screenCenter = WorldToScreen(worldCenter, vp, viewWidth, viewHeight);
    const float minRadiusPx = ScreenDistance(
        WorldToScreen(worldCenter + glm::vec2(minDistance, 0.0f), vp, viewWidth, viewHeight),
        screenCenter);
    const float maxRadiusPx = ScreenDistance(
        WorldToScreen(worldCenter + glm::vec2(maxDistance, 0.0f), vp, viewWidth, viewHeight),
        screenCenter);

    const ImU32 minRingColor = IM_COL32(80, 220, 175, 220);
    const ImU32 maxRingColor = IM_COL32(255, 185, 75, 220);
    if (minRadiusPx > 0.5f)
        drawList->AddCircle(screenCenter, minRadiusPx, minRingColor, 64, 1.5f);
    if (maxRadiusPx > 0.5f)
        drawList->AddCircle(screenCenter, maxRadiusPx, maxRingColor, 64, 1.5f);

    // Different directions keep both grips accessible when min and max are
    // equal. A minimum screen offset keeps zero-distance values editable.
    constexpr float kDiagonal = 0.70710678f;
    // Keep zero/small-distance grips clear of the fixed-size source icon drawn
    // over the object's center.
    constexpr float kMinAudioHandleOffsetPx = kComponentIconSizePx + 12.0f;
    const float minHandleOffset = std::max(minRadiusPx, kMinAudioHandleOffsetPx);
    const float maxHandleOffset = std::max(maxRadiusPx, kMinAudioHandleOffsetPx);
    const ImVec2 handles[2] = {
        ImVec2(screenCenter.x + minHandleOffset * kDiagonal,
               screenCenter.y - minHandleOffset * kDiagonal),
        ImVec2(screenCenter.x + maxHandleOffset, screenCenter.y),
    };

    int hovered = -1;
    if (m_dragAudioHandle < 0) {
        for (int i = 0; i < 2; ++i) {
            if (HitTest(io.MousePos, handles[i])) {
                hovered = i;
                break;
            }
        }
    }
    if (hovered >= 0) m_hoveredAudioHandle = hovered;

    if (io.MouseClicked[0] && hovered >= 0 && !ImGuizmo::IsUsing() &&
        m_dragHandle < 0 && m_dragCameraCorner < 0 && m_dragLightHandle < 0 &&
        m_dragAudioHandle < 0 && m_dragScaleHandle < 0) {
        m_dragAudioHandle = hovered;
        m_dragAudioId = source->GetID();
        m_dragStartMinDistance = source->GetMinDistance();
        m_dragStartMaxDistance = source->GetMaxDistance();
    }

    if (m_dragAudioHandle >= 0 && m_dragAudioId == source->GetID() && io.MouseDown[0]) {
        const glm::vec2 mouseWorld = ScreenToWorld(io.MousePos, vp, viewWidth, viewHeight);
        const float distance = glm::length(mouseWorld - worldCenter);
        if (m_dragAudioHandle == kAudioMinDistance) {
            source->SetMinDistance(std::min(distance, std::max(0.0f, source->GetMaxDistance())));
        } else if (m_dragAudioHandle == kAudioMaxDistance) {
            source->SetMaxDistance(std::max(distance, std::max(0.0f, source->GetMinDistance())));
        }
    }

    const ImU32 minHandleColor = IM_COL32(100, 245, 195, 255);
    const ImU32 maxHandleColor = IM_COL32(255, 205, 105, 255);
    const ImU32 hotColor = IM_COL32(255, 255, 255, 255);
    const bool draggingThis = m_dragAudioHandle >= 0 && m_dragAudioId == source->GetID();
    for (int i = 0; i < 2; ++i) {
        const bool isHot = i == hovered || (draggingThis && i == m_dragAudioHandle);
        const ImU32 color = isHot ? hotColor
                                  : (i == kAudioMinDistance ? minHandleColor : maxHandleColor);
        drawList->AddCircleFilled(handles[i], kHandleRadiusPx, color);
    }
}

void GizmosManager::CommitAudioSourceDrag() {
    Container* container = Engine::Get()->GetActiveContainer();
    Registry* registry = container ? container->FindSystem<Registry>() : nullptr;
    AudioSource* source = registry ? registry->Find<AudioSource>(m_dragAudioId) : nullptr;
    if (!source) return;

    std::vector<GizmoEdit> edits;
    if (source->GetMinDistance() != m_dragStartMinDistance) {
        edits.push_back({ source->GetID(), "minDistance", m_dragStartMinDistance,
                          source->GetMinDistance() });
    }
    if (source->GetMaxDistance() != m_dragStartMaxDistance) {
        edits.push_back({ source->GetID(), "maxDistance", m_dragStartMaxDistance,
                          source->GetMaxDistance() });
    }
    if (!edits.empty()) Notify(EDIT_COMMITTED_EVENT, edits);
}

// ─── Component-type icons ────────────────────────────────────────────────────
// To add a type, register the PNG stem and a component
// predicate here; lazy texture lookup, positioning, and alpha remain generic.
void GizmosManager::RegisterComponentIcons() {
    m_componentIcons.push_back({
        "camera_icon",
        [](GameObject* obj) { return obj->GetComponent<Camera>() != nullptr; },
        nullptr,
    });
    m_componentIcons.push_back({
        "particle_icons",
        [](GameObject* obj) { return obj->GetComponent<ParticleComponent>() != nullptr; },
        nullptr,
    });
    m_componentIcons.push_back({
        "light_icon",
        [](GameObject* obj) { return obj->GetComponent<Light>() != nullptr; },
        nullptr,
    });
    m_componentIcons.push_back({
        "shadow_caster_icon",
        [](GameObject* obj) { return obj->GetComponent<ShadowCaster>() != nullptr; },
        nullptr,
    });
    m_componentIcons.push_back({
        "audio_source_icon",
        [](GameObject* obj) { return obj->GetComponent<AudioSource>() != nullptr; },
        nullptr,
    });
}

void GizmosManager::DrawComponentIcons(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight) {
    if (!m_componentIconsRegistered) {
        RegisterComponentIcons();
        m_componentIconsRegistered = true;
    }
    if (m_componentIcons.empty()) return;

    Container* container = Engine::Get()->GetActiveContainer();
    if (!container) return;
    SceneManager* sceneManager = container->FindSystem<SceneManager>();
    if (!sceneManager) return;

    SelectionManager* selectionManager = container->FindSystem<SelectionManager>();

    // Fixed screen-space size, so icons stay the same on-screen regardless of
    // the Scene camera's ortho/zoom. Flip V (uv 0->1 top-to-bottom) because
    // textures load vertically flipped (stbi_set_flip_vertically_on_load), so
    // this cancels it and the icon renders upright in ImGui's y-down space.
    const float half = kComponentIconSizePx * 1.0f;
    const ImVec2 uv0(0.0f, 1.0f);
    const ImVec2 uv1(1.0f, 0.0f);
    const float gap = 4.0f;   // horizontal spacing when an object stacks several icons

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    glm::mat4 vp = proj * view;

    for (Scene* scene : sceneManager->GetScenes()) {
        if (!scene) continue;
        for (GameObject* obj : scene->GetAllGameObjects()) {
            if (!obj || !obj->GetActive()) continue;
            Transform* transform = obj->GetComponent<Transform>();
            if (!transform) continue;

            // Collect the resolved textures this object should show.
            std::vector<Texture2D*> icons;
            for (ComponentIcon& icon : m_componentIcons) {
                if (!icon.matches(obj)) continue;
                if (!icon.texture) icon.texture = AssetManager::Get().GetTextureByName(icon.textureName);
                if (icon.texture) icons.push_back(icon.texture);
            }
            if (icons.empty()) continue;

            // 75% alpha when this object is selected, 25% otherwise.
            int alpha = (selectionManager && selectionManager->IsSelected(obj->GetID()))
                            ? 191 : 64;
            ImU32 tint = IM_COL32(255, 255, 255, alpha);

            // Lay the icons out as a horizontal row centered on the transform.
            // Each icon gets a uniform square slot (side 2*half); the image is
            // fitted inside it preserving the texture's aspect ratio so a
            // non-square icon isn't stretched.
            ImVec2 center = WorldToScreen(transform->GetWorldPosition(), vp, viewWidth, viewHeight);
            float slot = 2.0f * half;
            float totalW = icons.size() * slot + (icons.size() - 1) * gap;
            float cx = center.x - totalW * 0.5f + half;   // center of the first slot
            for (Texture2D* tex : icons) {
                // Fit the texture inside a half x half box without stretching.
                float hw = half, hh = half;
                float tw = static_cast<float>(tex->GetWidth());
                float th = static_cast<float>(tex->GetHeight());
                if (tw > 0.0f && th > 0.0f) {
                    if (tw >= th) hh = half * (th / tw);   // wider than tall -> shrink height
                    else          hw = half * (tw / th);   // taller than wide -> shrink width
                }
                ImVec2 pMin(cx - hw, center.y - hh);
                ImVec2 pMax(cx + hw, center.y + hh);
                drawList->AddImage((ImTextureID)tex->GetTextureID(), pMin, pMax, uv0, uv1, tint);
                cx += slot + gap;
            }
        }
    }
}

ImVec2 GizmosManager::WorldToScreen(const glm::vec2& world, const glm::mat4& vp, float viewWidth, float viewHeight) {
    glm::vec4 clip = vp * glm::vec4(world, 0.0f, 1.0f);
    float ndcX = clip.x / clip.w;
    float ndcY = clip.y / clip.w;
    float sx = (ndcX * 0.5f + 0.5f) * viewWidth;
    float sy = (1.0f - (ndcY * 0.5f + 0.5f)) * viewHeight;
    return ImVec2(sx, sy);
}

glm::vec2 GizmosManager::ScreenToWorld(const ImVec2& screen, const glm::mat4& vp, float viewWidth, float viewHeight) {
    float ndcX = (screen.x / viewWidth) * 2.0f - 1.0f;
    float ndcY = 1.0f - (screen.y / viewHeight) * 2.0f;
    glm::mat4 invVP = glm::inverse(vp);
    glm::vec4 world = invVP * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
    return glm::vec2(world.x / world.w, world.y / world.w);
}

void GizmosManager::DrawBoxColliderGizmo(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight,
                                          Transform* transform, BoxCollider* boxCollider) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    glm::vec2 center = boxCollider->GetCenter();
    glm::vec2 size = boxCollider->GetSize();

    // Compute the 4 corners of the box in local space (relative to transform)
    glm::vec2 halfSize = size * 0.5f;
    glm::vec2 localCorners[4] = {
        center + glm::vec2(-halfSize.x, -halfSize.y),  // bottom-left
        center + glm::vec2( halfSize.x, -halfSize.y),  // bottom-right
        center + glm::vec2( halfSize.x,  halfSize.y),  // top-right
        center + glm::vec2(-halfSize.x,  halfSize.y),  // top-left
    };

    // Transform corners to world space using the object's world matrix
    glm::mat4 worldMat = transform->GetWorldMatrix();
    glm::mat4 vp = proj * view;

    ImVec2 screenCorners[4];
    for (int i = 0; i < 4; i++) {
        glm::vec4 worldPos = worldMat * glm::vec4(localCorners[i], 0.0f, 1.0f);
        screenCorners[i] = WorldToScreen(glm::vec2(worldPos), vp, viewWidth, viewHeight);
    }

    // Draw the collider outline
    ImU32 outlineColor = IM_COL32(0, 255, 0, 200);
    ImU32 handleColor = IM_COL32(0, 255, 0, 255);
    ImU32 handleHoverColor = IM_COL32(100, 255, 100, 255);
    float lineThickness = 2.0f;
    float handleRadius = 4.0f;

    for (int i = 0; i < 4; i++) {
        drawList->AddLine(screenCorners[i], screenCorners[(i + 1) % 4], outlineColor, lineThickness);
    }

    // Compute edge midpoints in screen space
    ImVec2 edgeMids[4];
    for (int i = 0; i < 4; i++) {
        edgeMids[i] = ImVec2(
            (screenCorners[i].x + screenCorners[(i + 1) % 4].x) * 0.5f,
            (screenCorners[i].y + screenCorners[(i + 1) % 4].y) * 0.5f
        );
    }

    // Collect all 8 handles: 0-3 = corners, 4-7 = edge midpoints
    ImVec2 handles[8];
    for (int i = 0; i < 4; i++) handles[i] = screenCorners[i];
    for (int i = 0; i < 4; i++) handles[i + 4] = edgeMids[i];

    // Hit-test handles
    ImVec2 mousePos = io.MousePos;
    int hoveredHandle = -1;
    for (int i = 0; i < 8; i++) {
        float dx = mousePos.x - handles[i].x;
        float dy = mousePos.y - handles[i].y;
        if (dx * dx + dy * dy < (handleRadius + 4.0f) * (handleRadius + 4.0f)) {
            hoveredHandle = i;
            break;
        }
    }
    if (hoveredHandle >= 0) m_hoveredHandle = hoveredHandle;

    // Handle mouse press - start dragging. Guard on m_dragHandle < 0 so only one
    // collider claims the drag when several are drawn in the same frame.
    if (io.MouseClicked[0] && !ImGuizmo::IsUsing() && m_dragHandle < 0 &&
        m_dragAudioHandle < 0 && hoveredHandle >= 0) {
        m_dragHandle = hoveredHandle;
        m_dragColliderId = boxCollider->GetID();
        m_dragStartMouseWorld = ScreenToWorld(mousePos, vp, viewWidth, viewHeight);
        m_dragStartCenter = center;
        m_dragStartSize = size;
    }

    // Process dragging — only for the collider that owns the active drag.
    if (m_dragHandle >= 0 && m_dragColliderId == boxCollider->GetID() && io.MouseDown[0]) {
        glm::vec2 currentMouseWorld = ScreenToWorld(mousePos, vp, viewWidth, viewHeight);

        // Convert mouse world position to local space of the transform
        glm::mat4 invWorld = glm::inverse(worldMat);
        glm::vec4 localMouse4 = invWorld * glm::vec4(currentMouseWorld, 0.0f, 1.0f);
        glm::vec2 localMouse(localMouse4.x, localMouse4.y);

        glm::vec4 startLocal4 = invWorld * glm::vec4(m_dragStartMouseWorld, 0.0f, 1.0f);
        glm::vec2 startLocal(startLocal4.x, startLocal4.y);

        glm::vec2 deltaLocal = localMouse - startLocal;

        glm::vec2 newCenter = m_dragStartCenter;
        glm::vec2 newSize = m_dragStartSize;
        glm::vec2 startHalfSize = m_dragStartSize * 0.5f;

        // min = center - halfSize, max = center + halfSize
        float minX = m_dragStartCenter.x - startHalfSize.x;
        float maxX = m_dragStartCenter.x + startHalfSize.x;
        float minY = m_dragStartCenter.y - startHalfSize.y;
        float maxY = m_dragStartCenter.y + startHalfSize.y;

        // corners: 0=BL, 1=BR, 2=TR, 3=TL
        // edges:   4=bottom, 5=right, 6=top, 7=left
        switch (m_dragHandle) {
            case 0: // bottom-left corner
                minX += deltaLocal.x;
                minY += deltaLocal.y;
                break;
            case 1: // bottom-right corner
                maxX += deltaLocal.x;
                minY += deltaLocal.y;
                break;
            case 2: // top-right corner
                maxX += deltaLocal.x;
                maxY += deltaLocal.y;
                break;
            case 3: // top-left corner
                minX += deltaLocal.x;
                maxY += deltaLocal.y;
                break;
            case 4: // bottom edge
                minY += deltaLocal.y;
                break;
            case 5: // right edge
                maxX += deltaLocal.x;
                break;
            case 6: // top edge
                maxY += deltaLocal.y;
                break;
            case 7: // left edge
                minX += deltaLocal.x;
                break;
        }

        // Alt: constrain to a uniform (aspect-preserving) resize. Drive both axes by
        // the factor of the axis being changed, anchored so the handle opposite the
        // dragged one stays fixed (corners keep the opposite corner; edges keep the
        // opposite edge and stay centred on the perpendicular axis).
        if (m_uniformScale) {
            const float startMinX = m_dragStartCenter.x - startHalfSize.x;
            const float startMaxX = m_dragStartCenter.x + startHalfSize.x;
            const float startMinY = m_dragStartCenter.y - startHalfSize.y;
            const float startMaxY = m_dragStartCenter.y + startHalfSize.y;

            const float fx = (m_dragStartSize.x > 1e-4f) ? (maxX - minX) / m_dragStartSize.x : 1.0f;
            const float fy = (m_dragStartSize.y > 1e-4f) ? (maxY - minY) / m_dragStartSize.y : 1.0f;
            float f = (std::abs(fx - 1.0f) >= std::abs(fy - 1.0f)) ? fx : fy;
            f = std::max(f, 0.001f);
            const glm::vec2 uni = m_dragStartSize * f;

            switch (m_dragHandle) {
                case 0: maxX = startMaxX; minX = startMaxX - uni.x; maxY = startMaxY; minY = startMaxY - uni.y; break; // BL, anchor TR
                case 1: minX = startMinX; maxX = startMinX + uni.x; maxY = startMaxY; minY = startMaxY - uni.y; break; // BR, anchor TL
                case 2: minX = startMinX; maxX = startMinX + uni.x; minY = startMinY; maxY = startMinY + uni.y; break; // TR, anchor BL
                case 3: maxX = startMaxX; minX = startMaxX - uni.x; minY = startMinY; maxY = startMinY + uni.y; break; // TL, anchor BR
                case 4: maxY = startMaxY; minY = startMaxY - uni.y; minX = m_dragStartCenter.x - uni.x * 0.5f; maxX = m_dragStartCenter.x + uni.x * 0.5f; break; // bottom, anchor top
                case 5: minX = startMinX; maxX = startMinX + uni.x; minY = m_dragStartCenter.y - uni.y * 0.5f; maxY = m_dragStartCenter.y + uni.y * 0.5f; break; // right, anchor left
                case 6: minY = startMinY; maxY = startMinY + uni.y; minX = m_dragStartCenter.x - uni.x * 0.5f; maxX = m_dragStartCenter.x + uni.x * 0.5f; break; // top, anchor bottom
                case 7: maxX = startMaxX; minX = startMaxX - uni.x; minY = m_dragStartCenter.y - uni.y * 0.5f; maxY = m_dragStartCenter.y + uni.y * 0.5f; break; // left, anchor right
            }
        }

        newSize = glm::vec2(maxX - minX, maxY - minY);
        newCenter = glm::vec2((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);

        // Enforce minimum size
        if (newSize.x >= 0.01f && newSize.y >= 0.01f) {
            boxCollider->SetSize(newSize);
            boxCollider->SetCenter(newCenter);
        }
    }

    // Draw handles
    for (int i = 0; i < 8; i++) {
        bool isHovered = (i == hoveredHandle) ||
                         (m_dragColliderId == boxCollider->GetID() && i == m_dragHandle);
        ImU32 color = isHovered ? handleHoverColor : handleColor;
        if (i < 4) {
            // Corner handles: filled squares
            drawList->AddRectFilled(
                ImVec2(handles[i].x - handleRadius, handles[i].y - handleRadius),
                ImVec2(handles[i].x + handleRadius, handles[i].y + handleRadius),
                color);
        } else {
            // Edge handles: filled circles
            drawList->AddCircleFilled(handles[i], handleRadius, color);
        }
    }

    // Draw center crosshair
    ImVec2 screenCenter = WorldToScreen(
        glm::vec2(worldMat * glm::vec4(center, 0.0f, 1.0f)), vp, viewWidth, viewHeight);
    drawList->AddLine(
        ImVec2(screenCenter.x - 6, screenCenter.y),
        ImVec2(screenCenter.x + 6, screenCenter.y),
        outlineColor, 1.0f);
    drawList->AddLine(
        ImVec2(screenCenter.x, screenCenter.y - 6),
        ImVec2(screenCenter.x, screenCenter.y + 6),
        outlineColor, 1.0f);
}

// ─── CircleCollider Gizmo ───────────────────────────────────────────────────
// Handle 0 = right radius handle
void GizmosManager::DrawCircleColliderGizmo(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight,
                                             Transform* transform, CircleCollider* circleCollider) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    glm::vec2 center = circleCollider->GetCenter();
    float radius = circleCollider->GetRadius();

    glm::mat4 worldMat = transform->GetWorldMatrix();
    glm::mat4 vp = proj * view;

    // World-space center of the collider
    glm::vec2 worldCenter = glm::vec2(worldMat * glm::vec4(center, 0.0f, 1.0f));
    ImVec2 screenCenter = WorldToScreen(worldCenter, vp, viewWidth, viewHeight);

    // Physics (CircleCollider::CreateShape) and the debug renderer both scale a
    // circle's radius uniformly by the LARGEST axis, since a circle can't skew.
    // The gizmo must use the same scale — deriving it from the X axis alone made
    // the ring wrong under non-uniform object scale.
    glm::vec2 worldScale = transform->GetWorldScale();
    float uniformScale = std::max(std::abs(worldScale.x), std::abs(worldScale.y));
    float worldRadius = radius * uniformScale;

    glm::vec2 worldEdge = worldCenter + glm::vec2(worldRadius, 0.0f);
    ImVec2 screenEdge = WorldToScreen(worldEdge, vp, viewWidth, viewHeight);

    float screenRadius = std::sqrt(
        (screenEdge.x - screenCenter.x) * (screenEdge.x - screenCenter.x) +
        (screenEdge.y - screenCenter.y) * (screenEdge.y - screenCenter.y));

    // Draw circle outline
    ImU32 outlineColor = IM_COL32(0, 255, 0, 200);
    ImU32 handleColor = IM_COL32(0, 255, 0, 255);
    ImU32 handleHoverColor = IM_COL32(100, 255, 100, 255);
    float lineThickness = 2.0f;
    float handleSz = 4.0f;

    drawList->AddCircle(screenCenter, screenRadius, outlineColor, 64, lineThickness);

    // 4 handles at cardinal directions on the circumference (right, top, left, bottom)
    ImVec2 handles[4] = {
        ImVec2(screenCenter.x + screenRadius, screenCenter.y),       // 0: right
        ImVec2(screenCenter.x, screenCenter.y - screenRadius),       // 1: top
        ImVec2(screenCenter.x - screenRadius, screenCenter.y),       // 2: left
        ImVec2(screenCenter.x, screenCenter.y + screenRadius),       // 3: bottom
    };

    // Hit-test
    ImVec2 mousePos = io.MousePos;
    int hoveredHandle = -1;
    for (int i = 0; i < 4; i++) {
        float dx = mousePos.x - handles[i].x;
        float dy = mousePos.y - handles[i].y;
        if (dx * dx + dy * dy < (handleSz + 4.0f) * (handleSz + 4.0f)) {
            hoveredHandle = i;
            break;
        }
    }
    if (hoveredHandle >= 0) m_hoveredHandle = hoveredHandle;

    // Start drag — guard so only one collider claims the drag per frame.
    if (io.MouseClicked[0] && !ImGuizmo::IsUsing() && m_dragHandle < 0 &&
        m_dragAudioHandle < 0 && hoveredHandle >= 0) {
        m_dragHandle = hoveredHandle;
        m_dragColliderId = circleCollider->GetID();
        m_dragStartMouseWorld = ScreenToWorld(mousePos, vp, viewWidth, viewHeight);
        m_dragStartRadius = radius;
        m_dragStartCenter = center;
    }

    // Process drag — only the owning collider. Measure the mouse distance from the
    // center in WORLD space and divide by the same uniform scale used to draw and
    // simulate the circle, so a handle dragged to a screen point yields the local
    // radius that lands the ring exactly there (matches physics/debug, unlike the
    // old per-axis inverse-transform which drifted under non-uniform scale).
    if (m_dragHandle >= 0 && m_dragColliderId == circleCollider->GetID() && io.MouseDown[0]) {
        glm::vec2 currentMouseWorld = ScreenToWorld(mousePos, vp, viewWidth, viewHeight);
        float worldDist = glm::length(currentMouseWorld - worldCenter);
        float newRadius = (uniformScale > 1e-6f) ? worldDist / uniformScale : worldDist;
        if (newRadius >= 0.01f) {
            circleCollider->SetRadius(newRadius);
        }
    }

    // Draw handles
    for (int i = 0; i < 4; i++) {
        bool isHovered = (i == hoveredHandle) ||
                         (m_dragColliderId == circleCollider->GetID() && i == m_dragHandle);
        ImU32 color = isHovered ? handleHoverColor : handleColor;
        drawList->AddCircleFilled(handles[i], handleSz, color);
    }

    // Center crosshair
    drawList->AddLine(ImVec2(screenCenter.x - 6, screenCenter.y), ImVec2(screenCenter.x + 6, screenCenter.y), outlineColor, 1.0f);
    drawList->AddLine(ImVec2(screenCenter.x, screenCenter.y - 6), ImVec2(screenCenter.x, screenCenter.y + 6), outlineColor, 1.0f);
}

// ─── CapsuleCollider Gizmo ──────────────────────────────────────────────────
// Handles: 0=right, 1=left (radius), 2=top, 3=bottom (height)
void GizmosManager::DrawCapsuleColliderGizmo(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight,
                                              Transform* transform, CapsuleCollider* capsuleCollider) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    glm::vec2 center = capsuleCollider->GetCenter();
    float capsuleRadius = capsuleCollider->GetRadius();
    float capsuleHeight = capsuleCollider->GetHeight();

    glm::mat4 worldMat = transform->GetWorldMatrix();
    glm::mat4 vp = proj * view;

    // The capsule is vertical: two semicircles connected by lines
    // In local space: half-height along Y, radius along X
    // NOTE: CapsuleCollider::radius is full width (diameter); physics divides by 2.
    float actualRadius = capsuleRadius * 0.5f;
    float halfH = capsuleHeight * 0.5f;

    // Key local-space points (before transform scale)
    // Top cap center, bottom cap center
    glm::vec2 localTopCenter    = center + glm::vec2(0.0f,  halfH);
    glm::vec2 localBottomCenter = center + glm::vec2(0.0f, -halfH);

    // Connecting line endpoints
    glm::vec2 localTopRight    = center + glm::vec2( actualRadius,  halfH);
    glm::vec2 localTopLeft     = center + glm::vec2(-actualRadius,  halfH);
    glm::vec2 localBottomRight = center + glm::vec2( actualRadius, -halfH);
    glm::vec2 localBottomLeft  = center + glm::vec2(-actualRadius, -halfH);

    // Transform local-space point to screen via world matrix
    auto toScreen = [&](const glm::vec2& local) -> ImVec2 {
        glm::vec4 world4 = worldMat * glm::vec4(local, 0.0f, 1.0f);
        return WorldToScreen(glm::vec2(world4), vp, viewWidth, viewHeight);
    };

    ImVec2 sTopCenter    = toScreen(localTopCenter);
    ImVec2 sBottomCenter = toScreen(localBottomCenter);
    ImVec2 sTopRight     = toScreen(localTopRight);
    ImVec2 sTopLeft      = toScreen(localTopLeft);
    ImVec2 sBottomRight  = toScreen(localBottomRight);
    ImVec2 sBottomLeft   = toScreen(localBottomLeft);
    ImVec2 sCenter       = toScreen(center);

    // Compute screen-space radius for the semicircles
    float screenRadiusTop = std::sqrt(
        (sTopRight.x - sTopCenter.x) * (sTopRight.x - sTopCenter.x) +
        (sTopRight.y - sTopCenter.y) * (sTopRight.y - sTopCenter.y));
    float screenRadiusBottom = std::sqrt(
        (sBottomRight.x - sBottomCenter.x) * (sBottomRight.x - sBottomCenter.x) +
        (sBottomRight.y - sBottomCenter.y) * (sBottomRight.y - sBottomCenter.y));

    ImU32 outlineColor = IM_COL32(0, 255, 0, 200);
    ImU32 handleColor = IM_COL32(0, 255, 0, 255);
    ImU32 handleHoverColor = IM_COL32(100, 255, 100, 255);
    float lineThickness = 2.0f;
    float handleSz = 4.0f;

    // Draw connecting lines (left and right sides)
    drawList->AddLine(sTopRight, sBottomRight, outlineColor, lineThickness);
    drawList->AddLine(sTopLeft, sBottomLeft, outlineColor, lineThickness);

    // Draw top semicircle (arc from 0 to pi, but in screen space Y is flipped)
    // Screen-space: rotation angle of the capsule's local Y axis
    float capsuleAngle = 0.0f;
    {
        // Get the screen direction of the local Y axis to determine rotation
        ImVec2 dir;
        dir.x = sTopCenter.x - sBottomCenter.x;
        dir.y = sTopCenter.y - sBottomCenter.y;
        capsuleAngle = std::atan2(dir.y, dir.x);
    }

    // Draw semicircular arcs using path
    auto drawSemiCircle = [&](ImVec2 arcCenter, float arcRadius, float startAngle, float endAngle) {
        const int segments = 32;
        for (int i = 0; i < segments; i++) {
            float a0 = startAngle + (endAngle - startAngle) * (float)i / (float)segments;
            float a1 = startAngle + (endAngle - startAngle) * (float)(i + 1) / (float)segments;
            ImVec2 p0(arcCenter.x + std::cos(a0) * arcRadius, arcCenter.y + std::sin(a0) * arcRadius);
            ImVec2 p1(arcCenter.x + std::cos(a1) * arcRadius, arcCenter.y + std::sin(a1) * arcRadius);
            drawList->AddLine(p0, p1, outlineColor, lineThickness);
        }
    };

    // Top semicircle: arc from right to left (going over the top)
    // In screen coords, going "up" means negative Y
    float topArcStart = capsuleAngle - 3.14159265f * 0.5f;
    float topArcEnd   = capsuleAngle + 3.14159265f * 0.5f;
    drawSemiCircle(sTopCenter, screenRadiusTop, topArcStart, topArcEnd);

    // Bottom semicircle: arc from left to right (going under)
    float bottomArcStart = capsuleAngle + 3.14159265f * 0.5f;
    float bottomArcEnd   = capsuleAngle + 3.14159265f * 1.5f;
    drawSemiCircle(sBottomCenter, screenRadiusBottom, bottomArcStart, bottomArcEnd);

    // Handles: 0=right (radius), 1=left (radius), 2=top (height+radius), 3=bottom (height+radius)
    glm::vec2 localHandleRight  = center + glm::vec2( actualRadius,  0.0f);
    glm::vec2 localHandleLeft   = center + glm::vec2(-actualRadius,  0.0f);
    glm::vec2 localHandleTop    = center + glm::vec2(0.0f,  halfH + actualRadius);
    glm::vec2 localHandleBottom = center + glm::vec2(0.0f, -(halfH + actualRadius));

    ImVec2 handles[4] = {
        toScreen(localHandleRight),
        toScreen(localHandleLeft),
        toScreen(localHandleTop),
        toScreen(localHandleBottom),
    };

    // Hit-test
    ImVec2 mousePos = io.MousePos;
    int hoveredHandle = -1;
    for (int i = 0; i < 4; i++) {
        float dx = mousePos.x - handles[i].x;
        float dy = mousePos.y - handles[i].y;
        if (dx * dx + dy * dy < (handleSz + 4.0f) * (handleSz + 4.0f)) {
            hoveredHandle = i;
            break;
        }
    }
    if (hoveredHandle >= 0) m_hoveredHandle = hoveredHandle;

    // Start drag — guard so only one collider claims the drag per frame.
    if (io.MouseClicked[0] && !ImGuizmo::IsUsing() && m_dragHandle < 0 &&
        m_dragAudioHandle < 0 && hoveredHandle >= 0) {
        m_dragHandle = hoveredHandle;
        m_dragColliderId = capsuleCollider->GetID();
        m_dragStartMouseWorld = ScreenToWorld(mousePos, vp, viewWidth, viewHeight);
        m_dragStartRadius = capsuleRadius;
        m_dragStartHeight = capsuleHeight;
        m_dragStartCenter = center;
    }

    // Process drag — only the owning collider.
    if (m_dragHandle >= 0 && m_dragColliderId == capsuleCollider->GetID() && io.MouseDown[0]) {
        glm::vec2 currentMouseWorld = ScreenToWorld(mousePos, vp, viewWidth, viewHeight);
        glm::mat4 invWorld = glm::inverse(worldMat);
        glm::vec4 localMouse4 = invWorld * glm::vec4(currentMouseWorld, 0.0f, 1.0f);
        glm::vec2 localMouse(localMouse4.x, localMouse4.y);

        if (m_dragHandle <= 1) {
            // Radius handles (left/right): distance from center along X
            float newRadius = std::abs(localMouse.x - center.x);
            if (newRadius >= 0.01f) {
                const float full = newRadius * 2.0f;
                // Alt: scale height by the same factor to keep the capsule uniform.
                if (m_uniformScale && m_dragStartRadius > 1e-4f) {
                    const float f = full / m_dragStartRadius;
                    capsuleCollider->SetRadius(m_dragStartRadius * f);
                    capsuleCollider->SetHeight(std::max(0.01f, m_dragStartHeight * f));
                } else {
                    capsuleCollider->SetRadius(full);
                }
            }
        } else {
            // Height handles (top/bottom): distance from center along Y, minus radius
            float distY = std::abs(localMouse.y - center.y);
            float newHalfH = distY - actualRadius;
            float newHeight = std::max(0.01f, newHalfH * 2.0f);
            if (m_uniformScale && m_dragStartHeight > 1e-4f) {
                const float f = newHeight / m_dragStartHeight;
                capsuleCollider->SetHeight(newHeight);
                capsuleCollider->SetRadius(std::max(0.01f, m_dragStartRadius * f));
            } else {
                capsuleCollider->SetHeight(newHeight);
            }
        }
    }

    // Draw handles
    for (int i = 0; i < 4; i++) {
        bool isHovered = (i == hoveredHandle) ||
                         (m_dragColliderId == capsuleCollider->GetID() && i == m_dragHandle);
        ImU32 color = isHovered ? handleHoverColor : handleColor;
        if (i <= 1) {
            // Radius: square handles
            drawList->AddRectFilled(
                ImVec2(handles[i].x - handleSz, handles[i].y - handleSz),
                ImVec2(handles[i].x + handleSz, handles[i].y + handleSz),
                color);
        } else {
            // Height: circle handles
            drawList->AddCircleFilled(handles[i], handleSz, color);
        }
    }

    // Center crosshair
    drawList->AddLine(ImVec2(sCenter.x - 6, sCenter.y), ImVec2(sCenter.x + 6, sCenter.y), outlineColor, 1.0f);
    drawList->AddLine(ImVec2(sCenter.x, sCenter.y - 6), ImVec2(sCenter.x, sCenter.y + 6), outlineColor, 1.0f);
}
