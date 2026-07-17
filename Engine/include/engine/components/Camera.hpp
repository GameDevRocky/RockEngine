#pragma once
#include "engine/components/Component.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "engine/rendering/cameras/RenderCamera.hpp"
#include "yaml-cpp/yaml.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

// AUTHORED camera settings. Pure data: no GL, no matrices, no RenderCamera
// member. A RenderView owns the resolved RenderCamera and pulls these
// settings in each frame via ApplyTo() -- a RenderCamera member here would
// make two views rendering the same camera at different sizes fight over one
// set of viewport dims, and would be dead weight in the play-mode deep copy.
class Camera : public Component
{
public:
    static inline const Event PROJECTION_CHANGED_EVENT     = Camera::CreateEvent();
    static inline const Event ORTHO_SIZE_CHANGED_EVENT     = Camera::CreateEvent();
    static inline const Event CLEAR_FLAGS_CHANGED_EVENT    = Camera::CreateEvent();
    static inline const Event CLEAR_COLOR_CHANGED_EVENT    = Camera::CreateEvent();
    static inline const Event PRIORITY_CHANGED_EVENT       = Camera::CreateEvent();
    static inline const Event TARGET_ASPECT_CHANGED_EVENT  = Camera::CreateEvent();
    static inline const Event CULLING_MASK_CHANGED_EVENT   = Camera::CreateEvent();
    static inline const Event VIEWPORT_RECT_CHANGED_EVENT  = Camera::CreateEvent();
    static inline const Event TARGET_TEXTURE_CHANGED_EVENT = Camera::CreateEvent();

    // Highest-priority enabled Camera on an active GameObject, across every
    // loaded scene of the ACTIVE container. Ties -> first found. nullptr if
    // none. A scan, not a registry: ScenePass already does the equivalent
    // scan (GetComponent<SpriteRenderer>() over every GameObject) every
    // frame, so this is noise by comparison -- and it's a seam a CameraSystem
    // could slot in behind later without touching a call site.
    static Camera* GetMain();

    // Writes settings + the GameObject's world pose into rc. Does NOT touch
    // rc's pixel dims -- those belong to the RenderView. Not const:
    // GetGameObject()/GetTransform() aren't const-qualified anywhere in the
    // Component hierarchy.
    void ApplyTo(RenderCamera& rc);

    RenderCamera::Projection GetProjection() const { return projection; }
    void SetProjection(RenderCamera::Projection p);

    float GetOrthoSize() const { return orthoSize; }
    void SetOrthoSize(float v);

    RenderCamera::ClearFlags GetClearFlags() const { return clearFlags; }
    void SetClearFlags(RenderCamera::ClearFlags f);

    const glm::vec4& GetClearColor() const { return clearColor; }
    void SetClearColor(const glm::vec4& c);

    int GetPriority() const { return priority; }
    void SetPriority(int p);

    // <= 0 means free (fills the panel); Unity-style "16:9" is 16.0f/9.0f.
    float GetTargetAspect() const { return targetAspect; }
    void SetTargetAspect(float a);

    const glm::vec4& GetViewportRect() const { return viewportRect; }
    void SetViewportRect(const glm::vec4& r);

    // Sorting-layer NAME set this camera renders; empty == draw everything.
    const std::vector<std::string>& GetCullingLayers() const { return cullingLayers; }
    void SetCullingLayers(std::vector<std::string> layers);

    // Render-to-texture target asset id; empty == render to the view (screen).
    const std::string& GetTargetTextureID() const { return targetTexture_id; }
    void SetTargetTextureID(const std::string& id);

    Camera* Copy() override;
    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void Accept(IVisitor* v) override;
    // MUST be const: Component::GetTypeName() is `const = 0`, while
    // Serializable::GetTypeName() is a separate non-const virtual -- wrong
    // constness here silently fails to satisfy Component's pure virtual and
    // is a compile error, not a subtle bug.
    std::string GetTypeName() const override { return "Camera"; }

private:
    RenderCamera::Projection projection = RenderCamera::Projection::Orthographic;
    float orthoSize = 360.0f;   // same units as Transform::localPosition
    RenderCamera::ClearFlags clearFlags = RenderCamera::ClearFlags::SolidColor;
    glm::vec4 clearColor = { 30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 1.0f };
    int priority = 0;
    float targetAspect = 16.0f / 9.0f;
    glm::vec4 viewportRect = { 0.0f, 0.0f, 1.0f, 1.0f };
    std::vector<std::string> cullingLayers;
    std::string targetTexture_id;
};
