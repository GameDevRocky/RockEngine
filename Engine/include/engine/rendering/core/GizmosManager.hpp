#pragma once
#include "engine/core/System.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <any>
#include <string>
#include <vector>
#include <functional>
#include "imgui.h"
#include "ImGuizmo.h"

class Texture2D;
class GameObject;

// One committed gizmo edit: what changed, on what, from what, to what.
//
// The gizmo draw code writes its target every frame while a drag is in flight,
// which is right for live feedback but must never become one undo entry per frame.
// A GizmoEdit is emitted once per completed gesture instead.
struct GizmoEdit {
    std::string targetId;   // Transform / collider / Camera id
    std::string property;   // "localPosition" | "localRotation" | "localScale"
                            // | "size" | "center" | "radius" | "height" | "orthoSize"
    std::any before;
    std::any after;
};

class GizmosManager : public System {
public:
    enum class EditMode { Transform, Collider };

    // Payload: std::vector<GizmoEdit>. Emitted on drag release, never per frame.
    // GizmosManager deliberately does not touch the UndoSystem itself — it only
    // reports, and the editor decides whether the gesture is worth recording.
    static inline const Event EDIT_COMMITTED_EVENT = Observable::CreateEvent();

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

    // Whether the transform gizmo should snap to the grid during a drag. Set by
    // the editor from the real keyboard state each frame (ImGui's io.KeyCtrl is
    // unusable here -- no ImGui keyboard backend feeds it, so NewFrame clears it).
    void SetSnapRequested(bool v) { m_snapRequested = v; }

    // Whether scaling (transform gizmo and collider resize) should stay uniform.
    // Set from the editor's Alt-key state, same rationale as SetSnapRequested.
    void SetUniformScaleRequested(bool v) { m_uniformScale = v; }

    bool IsHandleHovered() const { return m_hoveredHandle >= 0 || m_hoveredCameraCorner >= 0; }
    bool IsDraggingHandle() const { return m_dragHandle >= 0 || m_dragCameraCorner >= 0; }
    bool WantsCaptureMouse() const {
        return m_hoveredHandle >= 0 || m_dragHandle >= 0 ||
               m_hoveredCameraCorner >= 0 || m_dragCameraCorner >= 0;
    }

    GizmosManager* Copy() override;
    GizmosManager* Copy(Container* container) override;

    ~GizmosManager() override = default;

private:
    GizmosManager() = default;
    GizmosManager(const GizmosManager&) = delete;
    GizmosManager& operator=(const GizmosManager&) = delete;

    void DrawTransformGizmo(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight);
    void DrawColliderGizmo(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight);
    // Orange outline for every enabled Camera showing the world region it
    // renders to the Game view. Non-interactive; drawn regardless of selection.
    void DrawCameraGizmos(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight);
    void DrawCameraGizmo(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight,
                         class Transform* transform, class Camera* camera, int alpha, float aspect);

    // ─── Component-type icons ────────────────────────────────────────────────
    // Fixed screen-space icon drawn at an object's transform for every
    // component type registered below. Expandable: add a component -> icon
    // mapping in RegisterComponentIcons() and it shows up here automatically.
    struct ComponentIcon {
        std::string textureName;                  // AssetManager texture name (icon PNG stem)
        std::function<bool(GameObject*)> matches; // true if this object should show the icon
        Texture2D* texture = nullptr;             // resolved lazily from AssetManager, then cached
    };
    void RegisterComponentIcons();   // populate m_componentIcons once
    void DrawComponentIcons(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight);
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
    bool m_snapRequested = false;   // grid snap held this frame (set by editor)
    bool m_uniformScale  = false;   // Alt held: constrain scaling to uniform

    // Single-object rotation snap needs a raw (unsnapped) accumulator: the pivot is
    // rebuilt from the object every frame and ImGuizmo applies rotation per-frame
    // (unlike translate/scale which it writes absolutely), so snapping the tiny
    // per-frame increment would round it back to zero and never turn. Reset at drag
    // start; final angle = startWorldRot + accumulator, then snapped.
    float m_dragRawRotDelta = 0.0f;
    int   m_dragAxis = 0;   // latched active op during a snap drag: 0 none,1 T,2 R,3 S

    // Component-type icon table (see ComponentIcon above). Registered on first
    // draw so it doesn't depend on Init() ordering vs. AssetManager load.
    std::vector<ComponentIcon> m_componentIcons;
    bool m_componentIconsRegistered = false;
    static constexpr float kComponentIconSizePx = 32.0f;

    // Camera view-rect corner drag state. Dragging a corner rescales the
    // camera's orthoSize (its zoom); all four corners follow since they're
    // derived from orthoSize + the Game view's aspect. Scoped by camera id so
    // only one of several drawn camera rects owns a drag at a time.
    int m_hoveredCameraCorner = -1;      // reset each frame; 0..3 when a corner is hovered
    int m_dragCameraCorner = -1;         // -1 = none
    std::string m_dragCameraId;          // which camera owns the active corner drag ("" = none)
    float m_dragStartOrthoSize = 0.0f;

    // Emit a finished gesture. No-ops when nothing actually moved, so a click that
    // merely grabs a handle without dragging records nothing.
    void CommitTransformDrag();
    void CommitColliderDrag();
    void CommitCameraDrag();

    // Decompose `world` and write it onto `transform` via the SetWorld* trio.
    void ApplyWorld(class Transform* transform, const glm::mat4& world);

    // Per-object state captured once at the start of a transform-gizmo drag.
    //
    // The capture has to happen BEFORE ImGuizmo::Manipulate(), because Manipulate
    // mutates the matrix it is given on the same frame the drag begins — by the time
    // IsUsing() first returns true the original value is already gone.
    //
    // Locals are what the undo entry restores (re-deriving world->local through a
    // parent drifts on every round trip); the world TRS is what the group delta is
    // applied to.
    struct TransformDragRecord {
        std::string transformId;
        glm::vec2 startLocalPos{0};
        float     startLocalRot = 0.0f;
        glm::vec2 startLocalScale{1};
        glm::vec2 startWorldPos{0};
        float     startWorldRot = 0.0f;
        glm::vec2 startWorldScale{1};
    };

    bool m_transformGizmoActive = false;
    std::vector<TransformDragRecord> m_dragRecords;
    // The matrix handed to ImGuizmo, persisted ACROSS FRAMES while a drag is live.
    //
    // ImGuizmo accumulates into the matrix you give it: HandleRotation composes only
    // the rotation since the *previous frame* (it resets mRotationAngleOrigin every
    // frame) onto whatever matrix it was passed. Rebuilding the pivot from the live
    // centroid each frame therefore threw away everything accumulated so far, and the
    // gizmo read back one frame's worth of angle — which is why rotating a
    // multi-selection just jittered back and forth instead of turning.
    glm::mat4 m_pivotMatrix{1.0f};
    // The pivot's world matrix at drag start. Every frame the group delta is
    // recomputed as pivotNow * inverse(this), so it is derived fresh from the
    // authoritative pivot rather than accumulated frame to frame — no drift over a
    // long drag.
    glm::mat4 m_dragStartPivotWorld{1.0f};

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
