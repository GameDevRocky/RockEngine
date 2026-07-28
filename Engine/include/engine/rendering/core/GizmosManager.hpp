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
    std::string targetId;   // Transform / collider / Camera / Light / ShadowCaster id
    std::string property;   // "localPosition" | "localRotation" | "localScale"
                            // | "size" | "center" | "radius" | "height" | "orthoSize"
                            // | "range" | "innerRadius" | "innerAngle" | "outerAngle"
                            // | "casterSize" | "casterRadius"
                            // ShadowCaster's size/radius get their own names because
                            // a collider already claimed the plain ones and the
                            // bridge resolves purely by property string.
    std::any before;
    std::any after;
};

class GizmosManager : public System {
public:
    // SpriteBox edits the sprite's on-screen rectangle directly -- corner/edge
    // handles resize it and dragging the body moves it -- exactly like the box
    // collider gizmo. It writes Transform scale/position, since a sprite's world
    // size IS its pixel size times the transform's scale.
    enum class EditMode { Transform, Collider, SpriteBox };

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

    bool IsHandleHovered() const {
        return m_hoveredHandle >= 0 || m_hoveredCameraCorner >= 0 ||
               m_hoveredLightHandle >= 0 || m_hoveredScaleHandle >= 0;
    }
    bool IsDraggingHandle() const {
        return m_dragHandle >= 0 || m_dragCameraCorner >= 0 ||
               m_dragLightHandle >= 0 || m_dragScaleHandle >= 0;
    }
    // Every hover/drag state MUST be represented here. SceneViewGui checks this
    // before running object picking on a click -- a handle missing from this list
    // gets its selection cleared out from under it the moment it is grabbed.
    bool WantsCaptureMouse() const {
        return m_hoveredHandle >= 0 || m_dragHandle >= 0 ||
               m_hoveredCameraCorner >= 0 || m_dragCameraCorner >= 0 ||
               m_hoveredLightHandle >= 0 || m_dragLightHandle >= 0 ||
               m_hoveredScaleHandle >= 0 || m_dragScaleHandle >= 0;
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
    // Box-editing for the selected object's sprite rect (EditMode::SpriteBox).
    void DrawSpriteBoxGizmo(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight);
    // Orange outline for every enabled Camera showing the world region it
    // renders to the Game view. Non-interactive; drawn regardless of selection.
    void DrawCameraGizmos(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight);
    void DrawCameraGizmo(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight,
                         class Transform* transform, class Camera* camera, int alpha, float aspect);

    // ─── Light + ShadowCaster gizmos ─────────────────────────────────────────
    // Drawn for EVERY enabled light/caster in the scene (like the camera rects),
    // dimmed when unselected, so a lighting setup is legible without clicking
    // through the hierarchy. Handles appear only on the selected object.
    void DrawLightGizmos(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight);
    void DrawPointLightGizmo(const glm::mat4& vp, float viewWidth, float viewHeight,
                             class Transform* transform, class Light* light, int alpha, bool interactive);
    void DrawSpotLightGizmo(const glm::mat4& vp, float viewWidth, float viewHeight,
                            class Transform* transform, class Light* light, int alpha, bool interactive);
    void DrawDirectionalLightGizmo(const glm::mat4& vp, float viewWidth, float viewHeight,
                                   class Transform* transform, class Light* light, int alpha);
    void DrawShadowCasterGizmo(const glm::mat4& vp, float viewWidth, float viewHeight,
                               class ShadowCaster* caster, int alpha, bool interactive);

    // ─── Joint gizmos ────────────────────────────────────────────────────────
    // Both anchors plus the line linking them, drawn only for SELECTED objects.
    // Read-only on purpose: anchors are not draggable, so this adds no hover/drag
    // state and stays out of WantsCaptureMouse (see the note there).
    void DrawJointGizmos(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight);
    void DrawJointGizmo(const glm::mat4& vp, float viewWidth, float viewHeight, class Joint* joint);

    // Thin white outline around every selected object's sprite quad. Read-only,
    // drawn for the WHOLE selection regardless of edit mode -- it is a "this is
    // selected" indicator, not a tool, so it coexists with whichever gizmo
    // (transform/collider/sprite-box) is also drawn for the primary object.
    void DrawSelectionOutlines(const glm::mat4& view, const glm::mat4& proj, float viewWidth, float viewHeight);

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

    // Light / ShadowCaster handle drag state. Scoped by component id so only one
    // of several drawn lights owns a drag at a time -- same shape as the camera
    // corner drag above.
    //
    // Handle ids are a single flat space across both component types (see
    // LightHandle in GizmosManager.cpp), so one pair of fields covers all of them.
    int         m_hoveredLightHandle = -1;   // reset each frame
    int         m_dragLightHandle    = -1;   // -1 = none
    std::string m_dragLightId;               // owning Light/ShadowCaster id ("" = none)
    float       m_dragStartRange       = 0.0f;
    float       m_dragStartInnerRadius = 0.0f;
    float       m_dragStartInnerAngle  = 0.0f;
    float       m_dragStartOuterAngle  = 0.0f;
    float       m_dragStartCasterRadius = 0.0f;
    glm::vec2   m_dragStartCasterSize{ 0.0f };

    // ─── SpriteBox drag state ────────────────────────────────────────────────
    // Handles 0-3 are corners, 4-7 edge midpoints, 8 is the body (move).
    //
    // The frame this drag solves in is captured ONCE at grab time. A sprite-box
    // resize writes both scale and position, so recomputing the object's matrix
    // each frame would feed the edit back into its own input and the box would
    // run away under the cursor.
    int         m_hoveredScaleHandle = -1;   // reset each frame
    int         m_dragScaleHandle    = -1;   // -1 = none
    std::string m_dragScaleId;               // owning Transform id ("" = none)
    glm::vec2   m_scaleDragStartScale{ 1.0f };
    glm::vec2   m_scaleDragStartLocalPos{ 0.0f };
    glm::vec2   m_scaleDragStartBoxMouse{ 0.0f };
    // Inverse of (parentWorld * T * R) -- the object's frame WITHOUT its own
    // scale, so the box's extent in it is exactly localScale * spriteSize.
    glm::mat4   m_scaleDragInvBox{ 1.0f };

    // Emit a finished gesture. No-ops when nothing actually moved, so a click that
    // merely grabs a handle without dragging records nothing.
    void CommitTransformDrag();
    void CommitColliderDrag();
    void CommitCameraDrag();
    void CommitLightDrag();
    void CommitSpriteBoxDrag();

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
