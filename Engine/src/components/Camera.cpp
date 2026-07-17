#include "engine/components/Camera.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/core/Scene.hpp"
#include "Engine.hpp"

Camera* Camera::GetMain()
{
    Container* container = Engine::Get()->GetActiveContainer();
    if (!container) return nullptr;
    SceneManager* sm = container->FindSystem<SceneManager>();
    if (!sm) return nullptr;

    Camera* best = nullptr;
    for (Scene* scene : sm->GetScenes())
    {
        if (!scene) continue;
        for (GameObject* obj : scene->GetAllGameObjects())
        {
            if (!obj || !obj->GetActive()) continue;
            Camera* cam = obj->GetComponent<Camera>();
            if (!cam || !cam->GetEnabled()) continue;
            if (!best || cam->GetPriority() > best->GetPriority()) best = cam;
        }
    }
    return best;
}

void Camera::ApplyTo(RenderCamera& rc)
{
    if (GameObject* obj = GetGameObject())
    {
        if (Transform* t = obj->GetTransform())
        {
            rc.SetPosition(t->GetWorldPosition());
            rc.SetRotation(t->GetWorldRotation());
        }
    }
    rc.SetProjection(projection);
    rc.SetOrthoSize(orthoSize);
    rc.SetZoom(1.0f);   // zoom is editor-navigation-only; a game camera never zooms
    rc.SetClearFlags(clearFlags);
    rc.SetClearColor(clearColor);
    rc.SetTargetAspect(targetAspect);
    rc.SetViewportRect(viewportRect);
    rc.SetCullingLayers({ cullingLayers.begin(), cullingLayers.end() });
}

void Camera::SetProjection(RenderCamera::Projection p)
{
    if (projection == p) return;
    projection = p;
    Notify(PROJECTION_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Camera::SetOrthoSize(float v)
{
    if (orthoSize == v) return;
    orthoSize = v;
    Notify(ORTHO_SIZE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Camera::SetClearFlags(RenderCamera::ClearFlags f)
{
    if (clearFlags == f) return;
    clearFlags = f;
    Notify(CLEAR_FLAGS_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Camera::SetClearColor(const glm::vec4& c)
{
    clearColor = c;
    Notify(CLEAR_COLOR_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Camera::SetPriority(int p)
{
    if (priority == p) return;
    priority = p;
    Notify(PRIORITY_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Camera::SetTargetAspect(float a)
{
    if (targetAspect == a) return;
    targetAspect = a;
    Notify(TARGET_ASPECT_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Camera::SetViewportRect(const glm::vec4& r)
{
    viewportRect = r;
    Notify(VIEWPORT_RECT_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Camera::SetCullingLayers(std::vector<std::string> layers)
{
    cullingLayers = std::move(layers);
    Notify(CULLING_MASK_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Camera::SetTargetTextureID(const std::string& id)
{
    if (targetTexture_id == id) return;
    targetTexture_id = id;
    Notify(TARGET_TEXTURE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

Camera* Camera::Copy()
{
    Camera* copy = new Camera();
    copy->id = id;
    copy->enabled = enabled;
    copy->gameobject_id = gameobject_id;
    copy->projection = projection;
    copy->orthoSize = orthoSize;
    copy->clearFlags = clearFlags;
    copy->clearColor = clearColor;
    copy->priority = priority;
    copy->targetAspect = targetAspect;
    copy->viewportRect = viewportRect;
    copy->cullingLayers = cullingLayers;
    copy->targetTexture_id = targetTexture_id;
    return copy;
}

YAML::Node Camera::Serialize()
{
    YAML::Node node = Component::Serialize();
    node["projection"] = static_cast<int>(projection);
    node["orthoSize"] = orthoSize;
    node["clearFlags"] = static_cast<int>(clearFlags);
    node["clearColor"][0] = clearColor.r;
    node["clearColor"][1] = clearColor.g;
    node["clearColor"][2] = clearColor.b;
    node["clearColor"][3] = clearColor.a;
    node["clearColor"].SetStyle(YAML::EmitterStyle::Flow);
    node["priority"] = priority;
    node["targetAspect"] = targetAspect;
    node["viewportRect"][0] = viewportRect.x;
    node["viewportRect"][1] = viewportRect.y;
    node["viewportRect"][2] = viewportRect.z;
    node["viewportRect"][3] = viewportRect.w;
    node["viewportRect"].SetStyle(YAML::EmitterStyle::Flow);
    node["cullingLayers"] = cullingLayers;
    node["cullingLayers"].SetStyle(YAML::EmitterStyle::Flow);
    node["targetTextureId"] = targetTexture_id;
    return node;
}

void Camera::Deserialize(const YAML::Node& node)
{
    Component::Deserialize(node);
    projection = static_cast<RenderCamera::Projection>(
        node["projection"].as<int>(static_cast<int>(RenderCamera::Projection::Orthographic)));
    orthoSize = node["orthoSize"].as<float>(360.0f);
    clearFlags = static_cast<RenderCamera::ClearFlags>(
        node["clearFlags"].as<int>(static_cast<int>(RenderCamera::ClearFlags::SolidColor)));
    if (node["clearColor"] && node["clearColor"].IsSequence() && node["clearColor"].size() == 4) {
        clearColor = glm::vec4(
            node["clearColor"][0].as<float>(30.0f / 255.0f),
            node["clearColor"][1].as<float>(30.0f / 255.0f),
            node["clearColor"][2].as<float>(30.0f / 255.0f),
            node["clearColor"][3].as<float>(1.0f));
    }
    priority = node["priority"].as<int>(0);
    targetAspect = node["targetAspect"].as<float>(16.0f / 9.0f);
    if (node["viewportRect"] && node["viewportRect"].IsSequence() && node["viewportRect"].size() == 4) {
        viewportRect = glm::vec4(
            node["viewportRect"][0].as<float>(0.0f),
            node["viewportRect"][1].as<float>(0.0f),
            node["viewportRect"][2].as<float>(1.0f),
            node["viewportRect"][3].as<float>(1.0f));
    }
    if (node["cullingLayers"] && node["cullingLayers"].IsSequence()) {
        cullingLayers = node["cullingLayers"].as<std::vector<std::string>>();
    }
    targetTexture_id = node["targetTextureId"].as<std::string>("");
    state = State::Loaded;
}

void Camera::Accept(IVisitor* v)
{
    v->Visit(this);
}
