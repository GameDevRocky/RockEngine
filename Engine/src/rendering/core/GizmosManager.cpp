#include "engine/rendering/core/GizmosManager.hpp"
#include "Engine.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "imgui.h"
#include <cmath>

GizmosManager* GizmosManager::instance = nullptr;

void GizmosManager::Init(){
}

void GizmosManager::Update(){
}

void GizmosManager::Shutdown(){
}

GizmosManager* GizmosManager::Copy() { return nullptr; }
GizmosManager* GizmosManager::Copy(Container* /*container*/) { return nullptr; }

void GizmosManager::DrawGizmos(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight) {
    m_hoveredHandle = -1;  // reset each frame; DrawBoxColliderGizmo sets it if applicable

    Container* container = Engine::Get()->GetActiveContainer();
    SelectionManager* selectionManager = container->FindSystem<SelectionManager>();
    if (!selectionManager->HasSelection()) {
        m_dragHandle = -1;
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
    GameObject* selectedObj = dynamic_cast<GameObject*>(selectionManager->GetSerializable());
    if (!selectedObj) return;
    Transform* transform = selectedObj->GetComponent<Transform>();

    glm::mat4 objectMatrix = transform->GetWorldMatrix();

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

    float* snapPtr = nullptr;

    ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(proj),
        op,
        ImGuizmo::LOCAL,
        glm::value_ptr(objectMatrix),
        nullptr,
        snapPtr);

    if (ImGuizmo::IsUsing()) {
        float matrixTranslation[3], matrixRotation[3], matrixScale[3];
        ImGuizmo::DecomposeMatrixToComponents(
            glm::value_ptr(objectMatrix),
            matrixTranslation,
            matrixRotation,
            matrixScale);

        transform->SetWorldPosition(glm::vec2(matrixTranslation[0], matrixTranslation[1]));
        transform->SetWorldRotation(matrixRotation[2]);
        transform->SetWorldScale(glm::vec2(matrixScale[0], matrixScale[1]));
    }
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

    // Project a local-space edge point through worldMat to get screen radius
    glm::vec2 localEdge = center + glm::vec2(radius, 0.0f);
    glm::vec2 worldEdge = glm::vec2(worldMat * glm::vec4(localEdge, 0.0f, 1.0f));
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

    // Process drag — only the owning collider. Distance from mouse to center in local space.
    if (m_dragHandle >= 0 && m_dragColliderId == circleCollider->GetID() && io.MouseDown[0]) {
        glm::vec2 currentMouseWorld = ScreenToWorld(mousePos, vp, viewWidth, viewHeight);
        glm::mat4 invWorld = glm::inverse(worldMat);
        glm::vec4 localMouse4 = invWorld * glm::vec4(currentMouseWorld, 0.0f, 1.0f);
        glm::vec2 localMouse(localMouse4.x, localMouse4.y);

        float newRadius = glm::length(localMouse - center);
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
                capsuleCollider->SetRadius(newRadius * 2.0f);
            }
        } else {
            // Height handles (top/bottom): distance from center along Y, minus radius
            float distY = std::abs(localMouse.y - center.y);
            float newHalfH = distY - actualRadius;
            float newHeight = std::max(0.01f, newHalfH * 2.0f);
            capsuleCollider->SetHeight(newHeight);
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