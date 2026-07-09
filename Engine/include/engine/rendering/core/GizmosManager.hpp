#pragma once
#include "engine/core/System.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include "imgui.h"
#include "ImGuizmo.h"

class GizmosManager : public System {
public:
    enum class EditMode { Transform, Collider };

    static GizmosManager* Get()
    {
        if (!instance) instance = new GizmosManager();
        return instance;
    }

    void Init() override;
    void Update() override;
    void Shutdown() override;

    void DrawGizmos(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight);

    void SetOperation(ImGuizmo::OPERATION op) { m_currentOperation = op; }
    ImGuizmo::OPERATION GetOperation() const { return m_currentOperation; }

    void SetEditMode(EditMode mode) { m_editMode = mode; }
    EditMode GetEditMode() const { return m_editMode; }

    bool IsHandleHovered() const { return m_hoveredHandle >= 0; }
    bool IsDraggingHandle() const { return m_dragHandle >= 0; }
    bool WantsCaptureMouse() const { return m_hoveredHandle >= 0 || m_dragHandle >= 0; }

    GizmosManager* Copy() override;
    GizmosManager* Copy(Container* container) override;

    ~GizmosManager() override = default;

private:
    GizmosManager() = default;
    GizmosManager(const GizmosManager&) = delete;
    GizmosManager& operator=(const GizmosManager&) = delete;

    void DrawTransformGizmo(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight);
    void DrawColliderGizmo(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight);
    void DrawBoxColliderGizmo(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight,
                              class Transform* transform, class BoxCollider* boxCollider);
    void DrawCircleColliderGizmo(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight,
                                 class Transform* transform, class CircleCollider* circleCollider);
    void DrawCapsuleColliderGizmo(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight,
                                  class Transform* transform, class CapsuleCollider* capsuleCollider);

    ImVec2 WorldToScreen(const glm::vec2& world, const glm::mat4& vp, float viewWidth, float viewHeight);
    glm::vec2 ScreenToWorld(const ImVec2& screen, const glm::mat4& vp, float viewWidth, float viewHeight);

    static GizmosManager* instance;
    ImGuizmo::OPERATION m_currentOperation = ImGuizmo::UNIVERSAL;
    EditMode m_editMode = EditMode::Transform;

    // Collider drag state
    int m_dragHandle = -1;           // -1 = none
    std::string m_dragColliderId;    // which collider owns the active drag ("" = none)
    int m_hoveredHandle = -1;        // tracks hovered handle for WantsCaptureMouse
    bool m_draggingCenter = false;
    glm::vec2 m_dragStartMouseWorld{0};
    glm::vec2 m_dragStartCenter{0};
    glm::vec2 m_dragStartSize{0};
    float m_dragStartRadius{0};
    float m_dragStartHeight{0};
};
