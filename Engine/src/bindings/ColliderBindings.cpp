#include "engine/bindings/PythonBindings.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Collider.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "Engine.hpp"

static Collider* FindCollider(GameObject* go) {
    if (auto* c = go->GetComponent<BoxCollider>())     return c;
    if (auto* c = go->GetComponent<CapsuleCollider>()) return c;
    if (auto* c = go->GetComponent<CircleCollider>())  return c;
    return nullptr;
}

void BindCollider(pybind11::module_& m) {

    pybind11::module_ collider_module = m.def_submodule("collider_module", "Base Collider Bindings");

    collider_module.def("get_center", [](const std::string& go_id) {

        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (Collider* c = FindCollider(go))
                return std::make_tuple(c->GetCenter().x, c->GetCenter().y);
        }
        return std::make_tuple(0.0f, 0.0f);
    });

    collider_module.def("set_center", [](const std::string& go_id, float x, float y) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (Collider* c = FindCollider(go))
                c->SetCenter({x, y});
        }
    });

    collider_module.def("get_density", [](const std::string& go_id) -> float {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (Collider* c = FindCollider(go))
                return c->GetDensity();
        }
        return 1.0f;
    });

    collider_module.def("set_density", [](const std::string& go_id, float val) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (Collider* c = FindCollider(go))
                c->SetDensity(val);
        }
    });

    collider_module.def("get_bounciness", [](const std::string& go_id) -> float {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (Collider* c = FindCollider(go))
                return c->GetBounciness();
        }
        return 0.0f;
    });

    collider_module.def("set_bounciness", [](const std::string& go_id, float val) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (Collider* c = FindCollider(go))
                c->SetBounciness(val);
        }
    });

    collider_module.def("get_friction", [](const std::string& go_id) -> float {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (Collider* c = FindCollider(go))
                return c->GetFriction();
        }
        return 0.0f;
    });

    collider_module.def("set_friction", [](const std::string& go_id, float val) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (Collider* c = FindCollider(go))
                c->SetFriction(val);
        }
    });

    collider_module.def("get_rolling_resistance", [](const std::string& go_id) -> float {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (Collider* c = FindCollider(go))
                return c->GetRollingResistance();
        }
        return 0.0f;
    });

    collider_module.def("set_rolling_resistance", [](const std::string& go_id, float val) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (Collider* c = FindCollider(go))
                c->SetRollingResistance(val);
        }
    });

    collider_module.def("get_is_sensor", [](const std::string& go_id) -> bool {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (Collider* c = FindCollider(go))
                return c->GetIsSensor();
        }
        return false;
    });

    collider_module.def("set_is_sensor", [](const std::string& go_id, bool val) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (Collider* c = FindCollider(go))
                c->SetIsSensor(val);
        }
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
}
