#include "engine/bindings/PythonBindings.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Light.hpp"
#include "Engine.hpp"

// Addressed by GameObject id like Camera and SpriteRenderer, not by component id:
// a GameObject carries at most one Light, so the owning object names it
// unambiguously. Every getter answers with the component's own default when the
// object or component is missing, which keeps a script reading a destroyed
// light's property from raising mid-frame.
void BindLight(pybind11::module_& m) {
    pybind11::module_ light_module = m.def_submodule("light_module", "Light Bindings");

    auto find = [](const std::string& id) -> Light* {
        GameObject* go = registry->Find<GameObject>(id);
        return go ? go->GetComponent<Light>() : nullptr;
    };

    // type: 0 = Point, 1 = Spot, 2 = Directional, 3 = Global (Light::LightType).
    light_module.def("set_type", [find](const std::string& id, int v) {
        if (Light* l = find(id)) l->SetType(static_cast<Light::LightType>(v));
    });
    light_module.def("get_type", [find](const std::string& id) {
        Light* l = find(id);
        return l ? static_cast<int>(l->GetType()) : 0;
    });

    light_module.def("set_color", [find](const std::string& id, float r, float g, float b, float a) {
        if (Light* l = find(id)) l->SetColor(glm::vec4(r, g, b, a));
    });
    light_module.def("get_color", [find](const std::string& id) {
        if (Light* l = find(id)) {
            const glm::vec4& c = l->GetColor();
            return std::make_tuple(c.r, c.g, c.b, c.a);
        }
        return std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f);
    });

    light_module.def("set_intensity", [find](const std::string& id, float v) {
        if (Light* l = find(id)) l->SetIntensity(v);
    });
    light_module.def("get_intensity", [find](const std::string& id) {
        Light* l = find(id);
        return l ? l->GetIntensity() : 1.0f;
    });

    light_module.def("set_range", [find](const std::string& id, float v) {
        if (Light* l = find(id)) l->SetRange(v);
    });
    light_module.def("get_range", [find](const std::string& id) {
        Light* l = find(id);
        return l ? l->GetRange() : 300.0f;
    });

    light_module.def("set_inner_radius", [find](const std::string& id, float v) {
        if (Light* l = find(id)) l->SetInnerRadius(v);
    });
    light_module.def("get_inner_radius", [find](const std::string& id) {
        Light* l = find(id);
        return l ? l->GetInnerRadius() : 0.0f;
    });

    light_module.def("set_falloff", [find](const std::string& id, float v) {
        if (Light* l = find(id)) l->SetFalloff(v);
    });
    light_module.def("get_falloff", [find](const std::string& id) {
        Light* l = find(id);
        return l ? l->GetFalloff() : 1.0f;
    });

    light_module.def("set_inner_angle", [find](const std::string& id, float v) {
        if (Light* l = find(id)) l->SetInnerAngle(v);
    });
    light_module.def("get_inner_angle", [find](const std::string& id) {
        Light* l = find(id);
        return l ? l->GetInnerAngle() : 25.0f;
    });

    light_module.def("set_outer_angle", [find](const std::string& id, float v) {
        if (Light* l = find(id)) l->SetOuterAngle(v);
    });
    light_module.def("get_outer_angle", [find](const std::string& id) {
        Light* l = find(id);
        return l ? l->GetOuterAngle() : 45.0f;
    });

    light_module.def("set_height", [find](const std::string& id, float v) {
        if (Light* l = find(id)) l->SetHeight(v);
    });
    light_module.def("get_height", [find](const std::string& id) {
        Light* l = find(id);
        return l ? l->GetHeight() : 50.0f;
    });

    light_module.def("set_normal_influence", [find](const std::string& id, float v) {
        if (Light* l = find(id)) l->SetNormalInfluence(v);
    });
    light_module.def("get_normal_influence", [find](const std::string& id) {
        Light* l = find(id);
        return l ? l->GetNormalInfluence() : 1.0f;
    });

    light_module.def("set_cast_shadows", [find](const std::string& id, bool v) {
        if (Light* l = find(id)) l->SetCastShadows(v);
    });
    light_module.def("get_cast_shadows", [find](const std::string& id) {
        Light* l = find(id);
        return l ? l->GetCastShadows() : false;
    });

    light_module.def("set_shadow_strength", [find](const std::string& id, float v) {
        if (Light* l = find(id)) l->SetShadowStrength(v);
    });
    light_module.def("get_shadow_strength", [find](const std::string& id) {
        Light* l = find(id);
        return l ? l->GetShadowStrength() : 1.0f;
    });

    // Read-only: direction is derived from the owning Transform's world rotation,
    // deliberately with no authored field behind it (see Light::GetWorldDirection).
    // A script that wants to aim a spot light rotates the transform.
    light_module.def("get_world_direction", [find](const std::string& id) {
        if (Light* l = find(id)) {
            const glm::vec2 d = l->GetWorldDirection();
            return std::make_tuple(d.x, d.y);
        }
        return std::make_tuple(1.0f, 0.0f);
    });
}
