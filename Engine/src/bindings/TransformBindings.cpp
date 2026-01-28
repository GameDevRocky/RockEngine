#include "engine/bindings/PythonBindings.hpp"

#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "Engine.hpp"
#include "engine/components/Transform.hpp"

void BindTransform(pybind11::module_& m) {
    pybind11::module_ transform_module = m.def_submodule("transform_module", "Transform Bindings");

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
}