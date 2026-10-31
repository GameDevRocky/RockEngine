#include "engine/bindings/PythonBindings.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Collider.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/ChainCollider.hpp"
#include "Engine.hpp"
#include <pybind11/stl.h>
#include <utility>

static Collider* FindCollider(const std::string& id) {
    if (auto* collider = registry->Find<Collider>(id)) return collider;
    GameObject* go = registry->Find<GameObject>(id);
    if (!go) return nullptr;
    if (auto* c = go->GetComponent<BoxCollider>())     return c;
    if (auto* c = go->GetComponent<CapsuleCollider>()) return c;
    if (auto* c = go->GetComponent<CircleCollider>())  return c;
    if (auto* c = go->GetComponent<ChainCollider>())   return c;
    return nullptr;
}

void BindCollider(pybind11::module_& m) {

    pybind11::module_ collider_module = m.def_submodule("collider_module", "Base Collider Bindings");

    collider_module.def("get_center", [](const std::string& go_id) {

        if (Collider* c = FindCollider(go_id))
            return std::make_tuple(c->GetCenter().x, c->GetCenter().y);
        return std::make_tuple(0.0f, 0.0f);
    });

    collider_module.def("set_center", [](const std::string& go_id, float x, float y) {
        if (Collider* c = FindCollider(go_id)) c->SetCenter({x, y});
    });

    collider_module.def("get_density", [](const std::string& go_id) -> float {
        if (Collider* c = FindCollider(go_id)) return c->GetDensity();
        return 1.0f;
    });

    collider_module.def("set_density", [](const std::string& go_id, float val) {
        if (Collider* c = FindCollider(go_id)) c->SetDensity(val);
    });

    collider_module.def("get_bounciness", [](const std::string& go_id) -> float {
        if (Collider* c = FindCollider(go_id)) return c->GetBounciness();
        return 0.0f;
    });

    collider_module.def("set_bounciness", [](const std::string& go_id, float val) {
        if (Collider* c = FindCollider(go_id)) c->SetBounciness(val);
    });

    collider_module.def("get_friction", [](const std::string& go_id) -> float {
        if (Collider* c = FindCollider(go_id)) return c->GetFriction();
        return 0.0f;
    });

    collider_module.def("set_friction", [](const std::string& go_id, float val) {
        if (Collider* c = FindCollider(go_id)) c->SetFriction(val);
    });

    collider_module.def("get_rolling_resistance", [](const std::string& go_id) -> float {
        if (Collider* c = FindCollider(go_id)) return c->GetRollingResistance();
        return 0.0f;
    });

    collider_module.def("set_rolling_resistance", [](const std::string& go_id, float val) {
        if (Collider* c = FindCollider(go_id)) c->SetRollingResistance(val);
    });

    collider_module.def("get_is_sensor", [](const std::string& go_id) -> bool {
        if (Collider* c = FindCollider(go_id)) return c->GetIsSensor();
        return false;
    });

    collider_module.def("set_is_sensor", [](const std::string& go_id, bool val) {
        if (Collider* c = FindCollider(go_id)) c->SetIsSensor(val);
    });

    // -----------------------------------------------------------------------
    // BoxCollider
    // -----------------------------------------------------------------------
    pybind11::module_ box_collider_module = m.def_submodule("box_collider_module", "BoxCollider Bindings");

    box_collider_module.def("get_size", [](const std::string& go_id) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (auto* c = go->GetComponent<BoxCollider>())
                return std::make_tuple(c->GetSize().x, c->GetSize().y);
        }
        return std::make_tuple(0.0f, 0.0f);
    });

    box_collider_module.def("set_size", [](const std::string& go_id, float w, float h) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (auto* c = go->GetComponent<BoxCollider>())
                c->SetSize({w, h});
        }
    });

    // -----------------------------------------------------------------------
    // CapsuleCollider
    // -----------------------------------------------------------------------
    pybind11::module_ capsule_collider_module = m.def_submodule("capsule_collider_module", "CapsuleCollider Bindings");

    capsule_collider_module.def("get_radius", [](const std::string& go_id) -> float {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (auto* c = go->GetComponent<CapsuleCollider>())
                return c->GetRadius();
        }
        return 0.0f;
    });

    capsule_collider_module.def("set_radius", [](const std::string& go_id, float val) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (auto* c = go->GetComponent<CapsuleCollider>())
                c->SetRadius(val);
        }
    });

    capsule_collider_module.def("get_height", [](const std::string& go_id) -> float {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (auto* c = go->GetComponent<CapsuleCollider>())
                return c->GetHeight();
        }
        return 0.0f;
    });

    capsule_collider_module.def("set_height", [](const std::string& go_id, float val) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (auto* c = go->GetComponent<CapsuleCollider>())
                c->SetHeight(val);
        }
    });

    // -----------------------------------------------------------------------
    // CircleCollider
    // -----------------------------------------------------------------------
    pybind11::module_ circle_collider_module = m.def_submodule("circle_collider_module", "CircleCollider Bindings");

    circle_collider_module.def("get_radius", [](const std::string& go_id) -> float {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (auto* c = go->GetComponent<CircleCollider>())
                return c->GetRadius();
        }
        return 0.0f;
    });

    circle_collider_module.def("set_radius", [](const std::string& go_id, float val) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (auto* c = go->GetComponent<CircleCollider>())
                c->SetRadius(val);
        }
    });

    // ChainCollider uses its component id so multiple outlines on one object
    // remain independently addressable by the Python API.
    pybind11::module_ chain_collider_module = m.def_submodule("chain_collider_module", "ChainCollider Bindings");
    chain_collider_module.def("get_points", [](const std::string& component_id) {
        std::vector<std::pair<float, float>> result;
        if (auto* collider = registry->Find<ChainCollider>(component_id)) {
            for (const glm::vec2& point : collider->GetPoints())
                result.emplace_back(point.x, point.y);
        }
        return result;
    });
    chain_collider_module.def("set_points", [](const std::string& component_id,
                                                const std::vector<std::pair<float, float>>& values) {
        if (auto* collider = registry->Find<ChainCollider>(component_id)) {
            std::vector<glm::vec2> points;
            points.reserve(values.size());
            for (const auto& [x, y] : values) points.emplace_back(x, y);
            collider->SetPoints(std::move(points));
        }
    });
    chain_collider_module.def("generate_from_sprite", [](const std::string& component_id, float alpha_threshold) {
        if (auto* collider = registry->Find<ChainCollider>(component_id))
            return collider->GenerateFromSprite(alpha_threshold);
        return false;
    });
    chain_collider_module.def("get_is_loop", [](const std::string& componentId) {
        if (auto* collider = registry->Find<ChainCollider>(componentId)) return collider->GetIsLoop();
        return false;
    });
    chain_collider_module.def("set_is_loop", [](const std::string& componentId, bool value) {
        if (auto* collider = registry->Find<ChainCollider>(componentId)) collider->SetIsLoop(value);
    });
    chain_collider_module.def("get_reverse_side", [](const std::string& componentId) {
        if (auto* collider = registry->Find<ChainCollider>(componentId)) return collider->GetReverseSide();
        return false;
    });
    chain_collider_module.def("set_reverse_side", [](const std::string& componentId, bool value) {
        if (auto* collider = registry->Find<ChainCollider>(componentId)) collider->SetReverseSide(value);
    });
    chain_collider_module.def("has_valid_geometry", [](const std::string& componentId) {
        if (auto* collider = registry->Find<ChainCollider>(componentId)) return collider->HasValidGeometry();
        return false;
    });
    chain_collider_module.def("get_alpha_threshold", [](const std::string& componentId) {
        if (auto* collider = registry->Find<ChainCollider>(componentId)) return collider->GetAlphaThreshold();
        return 0.1f;
    });
    chain_collider_module.def("set_alpha_threshold", [](const std::string& componentId, float value) {
        if (auto* collider = registry->Find<ChainCollider>(componentId)) collider->SetAlphaThreshold(value);
    });
}
