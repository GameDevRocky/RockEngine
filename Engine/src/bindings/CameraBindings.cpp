#include "engine/bindings/PythonBindings.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Camera.hpp"
#include "Engine.hpp"
#include <pybind11/stl.h>

void BindCamera(pybind11::module_& m) {
    pybind11::module_ camera_module = m.def_submodule("camera_module", "Camera Bindings");

    // The GameObject id of the resolved main camera (highest-priority enabled
    // Camera on an active GameObject), or "" if none exists.
    camera_module.def("get_main_camera_gameobject_id", []() -> std::string {
        Camera* cam = Camera::GetMain();
        if (!cam) return {};
        GameObject* go = cam->GetGameObject();
        return go ? go->GetID() : std::string{};
    });

    camera_module.def("set_ortho_size", [](const std::string& id, float v) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* cam = go->GetComponent<Camera>()) cam->SetOrthoSize(v);
    });

    camera_module.def("get_ortho_size", [](const std::string& id) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* cam = go->GetComponent<Camera>()) return cam->GetOrthoSize();
        return 360.0f;
    });

    camera_module.def("set_priority", [](const std::string& id, int v) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* cam = go->GetComponent<Camera>()) cam->SetPriority(v);
    });

    camera_module.def("get_priority", [](const std::string& id) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* cam = go->GetComponent<Camera>()) return cam->GetPriority();
        return 0;
    });

    // clearFlags encoded as int: 0=SolidColor, 1=DepthOnly, 2=Nothing (RenderCamera::ClearFlags).
    camera_module.def("set_clear_flags", [](const std::string& id, int v) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* cam = go->GetComponent<Camera>())
            cam->SetClearFlags(static_cast<RenderCamera::ClearFlags>(v));
    });

    camera_module.def("get_clear_flags", [](const std::string& id) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* cam = go->GetComponent<Camera>())
            return static_cast<int>(cam->GetClearFlags());
        return 0;
    });

    camera_module.def("set_clear_color", [](const std::string& id, float r, float g, float b, float a) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* cam = go->GetComponent<Camera>()) cam->SetClearColor(glm::vec4(r, g, b, a));
    });

    camera_module.def("get_clear_color", [](const std::string& id) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* cam = go->GetComponent<Camera>()) {
            const glm::vec4& c = cam->GetClearColor();
            return std::make_tuple(c.r, c.g, c.b, c.a);
        }
        return std::make_tuple(0.0f, 0.0f, 0.0f, 1.0f);
    });

    camera_module.def("set_target_aspect", [](const std::string& id, float v) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* cam = go->GetComponent<Camera>()) cam->SetTargetAspect(v);
    });

    camera_module.def("get_target_aspect", [](const std::string& id) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* cam = go->GetComponent<Camera>()) return cam->GetTargetAspect();
        return 0.0f;
    });

    camera_module.def("set_viewport_rect", [](const std::string& id, float x, float y, float w, float h) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* cam = go->GetComponent<Camera>()) cam->SetViewportRect(glm::vec4(x, y, w, h));
    });

    camera_module.def("get_viewport_rect", [](const std::string& id) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* cam = go->GetComponent<Camera>()) {
            const glm::vec4& r = cam->GetViewportRect();
            return std::make_tuple(r.x, r.y, r.z, r.w);
        }
        return std::make_tuple(0.0f, 0.0f, 1.0f, 1.0f);
    });

    camera_module.def("set_culling_layers", [](const std::string& id, std::vector<std::string> layers) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* cam = go->GetComponent<Camera>()) cam->SetCullingLayers(std::move(layers));
    });

    camera_module.def("get_culling_layers", [](const std::string& id) -> std::vector<std::string> {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* cam = go->GetComponent<Camera>()) return cam->GetCullingLayers();
        return {};
    });

    camera_module.def("set_target_texture_id", [](const std::string& id, const std::string& textureId) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* cam = go->GetComponent<Camera>()) cam->SetTargetTextureID(textureId);
    });

    camera_module.def("get_target_texture_id", [](const std::string& id) -> std::string {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* cam = go->GetComponent<Camera>()) return cam->GetTargetTextureID();
        return {};
    });
}
