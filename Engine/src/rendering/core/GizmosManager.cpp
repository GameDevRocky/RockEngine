#include "engine/rendering/core/GizmosManager.hpp"
#include "Engine.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/components/Camera.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/core/Scene.hpp"
#include "engine/rendering/Renderer.hpp"
#include "engine/rendering/views/GameRenderView.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "imgui.h"
#include <cmath>

GizmosManager* GizmosManager::instance = nullptr;

namespace {
    // Grid-snap increments. Translation uses GridCellPixels (world units == pixels)
    // so objects land on grid lines; rotation snaps to a fixed angle; scale snaps
    // the drag RATIO (1.0-relative), never the absolute scale, so it can't collapse.
    constexpr float kRotateSnapDeg = 15.0f;
    constexpr float kScaleSnapStep = 0.25f;
    // Thresholds that decide which single operation a drag frame is exercising.
    constexpr float kPosActiveEps = 0.25f;   // world units
    constexpr float kRotActiveEps = 0.05f;   // degrees

    float SnapTo(float v, float step) { return std::round(v / step) * step; }
    glm::vec2 SnapTo(const glm::vec2& v, float step) { return { SnapTo(v.x, step), SnapTo(v.y, step) }; }
}

void GizmosManager::Init(){
}

void GizmosManager::Update(){
}

void GizmosManager::Shutdown(){
}

GizmosManager* GizmosManager::Copy() { return nullptr; }
GizmosManager* GizmosManager::Copy(Container* /*container*/) { return nullptr; }

void GizmosManager::DrawGizmos(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight) {
    m_hoveredHandle = -1;        // reset each frame; DrawBoxColliderGizmo sets it if applicable
    m_hoveredCameraCorner = -1;  // reset each frame; DrawCameraGizmo sets it if applicable

    // Camera view-region rects and component icons are non-interactive and
    // independent of selection, so draw them first -- before the no-selection
    // early-return below.
    DrawCameraGizmos(view, proj, viewWidth, viewHeight);
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
        return;
    }

    if (m_editMode == EditMode::Collider) {
        DrawColliderGizmo(view, proj, viewWidth, viewHeight);
    } else {
        DrawTransformGizmo(view, proj, viewWidth, viewHeight);
    }
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
                    if (m_snapRequested) pos = SnapTo(pos, EngineUtils::RenderUtils::GridCellPixels);
                } else if (m_dragAxis == 2) {    // rotate (Alt has no effect)
                    if (m_snapRequested) rot = SnapTo(rec.startWorldRot + m_dragRawRotDelta, kRotateSnapDeg);
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
                        ratio.x = std::max(SnapTo(std::abs(ratio.x), kScaleSnapStep), kScaleSnapStep) * (ratio.x < 0 ? -1.f : 1.f);
                        ratio.y = std::max(SnapTo(std::abs(ratio.y), kScaleSnapStep), kScaleSnapStep) * (ratio.y < 0 ? -1.f : 1.f);
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
                        deltaPos = SnapTo(startCentroid + deltaPos, EngineUtils::RenderUtils::GridCellPixels) - startCentroid;
                } else if (std::abs(deltaRot) > kRotActiveEps) {
                    if (m_snapRequested) deltaRot = SnapTo(deltaRot, kRotateSnapDeg);
                } else {
                    if (m_uniformScale) {
                        const float f = (std::abs(deltaScale.x - 1.f) >= std::abs(deltaScale.y - 1.f))
                                            ? deltaScale.x : deltaScale.y;
                        deltaScale = glm::vec2(f, f);
                    }
                    if (m_snapRequested) {
                        deltaScale.x = std::max(SnapTo(deltaScale.x, kScaleSnapStep), kScaleSnapStep);
                        deltaScale.y = std::max(SnapTo(deltaScale.y, kScaleSnapStep), kScaleSnapStep);
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

// ─── Component-type icons ────────────────────────────────────────────────────
// To add an icon for a new component type: drop its PNG in Domain's icons
// folder (registered by AssetManager under the file stem) and add one entry
// here mapping that texture name to a "does this object have the component?"
// predicate. Everything else -- lookup, positioning, alpha -- is generic.
void GizmosManager::RegisterComponentIcons() {
    m_componentIcons.push_back({
        "camera_icon",
        [](GameObject* obj) { return obj->GetComponent<Camera>() != nullptr; },
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
    if (io.MouseClicked[0] && !ImGuizmo::IsUsing() && m_dragHandle < 0 && hoveredHandle >= 0) {
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
    if (io.MouseClicked[0] && !ImGuizmo::IsUsing() && m_dragHandle < 0 && hoveredHandle >= 0) {
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
    if (io.MouseClicked[0] && !ImGuizmo::IsUsing() && m_dragHandle < 0 && hoveredHandle >= 0) {
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