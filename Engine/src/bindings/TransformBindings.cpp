#include "engine/bindings/PythonBindings.hpp"

#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "Engine.hpp"
#include "engine/components/Transform.hpp"

void BindTransform(pybind11::module_& m) {
    pybind11::module_ transform_module = m.def_submodule("transform_module", "Transform Bindings");

    transform_module.def("set_parent", [](const std::string& childId, pybind11::object parentId, bool keepWorld) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();

        GameObject* childGo = registry->Find<GameObject>(childId);
        if (!childGo) {
            return;
        }

        Transform* child = childGo->GetComponent<Transform>();
        if (!child) {
            return;
        }

        if (parentId.is_none()) {
            child->SetParent(nullptr, keepWorld);
            return;
        }

        std::string parentIdStr = parentId.cast<std::string>();
        GameObject* parentGo = registry->Find<GameObject>(parentIdStr);
        if (!parentGo) {
            return;
        }

        Transform* parent = parentGo->GetComponent<Transform>();
        if (!parent) {
            return;
        }

        child->SetParent(parent, keepWorld);
    }, pybind11::arg("child_id"), pybind11::arg("parent_id") = pybind11::none(), pybind11::arg("keep_world") = true);

    transform_module.def("get_parent", [](const std::string& childId) -> pybind11::object {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();

        GameObject* childGo = registry->Find<GameObject>(childId);
        if (!childGo) {
            return pybind11::none();
        }

        Transform* child = childGo->GetComponent<Transform>();
        if (!child) {
            return pybind11::none();
        }

        Transform* parent = child->GetParent();
        if (!parent) {
            return pybind11::none();
        }

        GameObject* parentGo = parent->GetGameObject();
        if (!parentGo) {
            return pybind11::none();
        }

        return pybind11::str(parentGo->GetID());
    });

    transform_module.def("set_position", [](const std::string& id, float x, float y) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                t->SetPosition({x, y});
            }
        }
    });
    transform_module.def("set_world_position", [](const std::string& id, float x, float y) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                t->SetWorldPosition({x, y});
            }
        }
    });
    
    transform_module.def("get_position", [](const std::string& id) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                return std::make_tuple(t->localPosition.x, t->localPosition.y);
            }
        }
        return std::make_tuple(0.0f, 0.0f);
    });

    transform_module.def("get_world_position", [](const std::string& id) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                const auto& pos = t->GetWorldPosition();
                return std::make_tuple(pos.x, pos.y);
            }
        }
        return std::make_tuple(0.0f, 0.0f);
    });

    transform_module.def("set_rotation", [](const std::string& id, float degrees) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                t->SetRotation(degrees);
            }
        }
    });

    transform_module.def("set_world_rotation", [](const std::string& id, float degrees) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                t->SetWorldRotation(degrees);
            }
        }
    });

    transform_module.def("get_rotation", [](const std::string& id) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                return t->localRotation;
            }
        }
        return 0.0f;
    });

    transform_module.def("get_world_rotation", [](const std::string& id) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                return t->GetWorldRotation();
            }
        }
        return 0.0f;
    });

    transform_module.def("set_scale", [](const std::string& id, float x, float y) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                t->SetScale({x, y});
            }
        }
    });

    transform_module.def("set_world_scale", [](const std::string& id, float x, float y) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                t->SetWorldScale({x, y});
            }
        }
    });

    transform_module.def("get_scale", [](const std::string& id) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                return std::make_tuple(t->localScale.x, t->localScale.y);
            }
        }
        return std::make_tuple(1.0f, 1.0f);
    });

    transform_module.def("get_world_scale", [](const std::string& id) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                const auto& scale = t->GetWorldScale();
                return std::make_tuple(scale.x, scale.y);
            }
        }
        return std::make_tuple(1.0f, 1.0f);
    });
}