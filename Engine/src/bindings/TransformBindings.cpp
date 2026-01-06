#include "engine/bindings/PythonBindings.hpp"

#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"

void BindTransform(pybind11::module_& m) {
    // TRANSFORM API
    // --- TRANSFORM API ---
    
    // Set Position (vec2)
    m.def("set_position", [](const std::string& id, float x, float y) {
        GameObject* go = Registry::Get().Find<GameObject>(id); 
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                t->SetPosition({x, y});
            }
        }
    });

    m.def("get_position", [](const std::string& id) {
        GameObject* go = Registry::Get().Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                return std::make_tuple(t->localPosition.x, t->localPosition.y);
            }
        }
        return std::make_tuple(0.0f, 0.0f);
    });

    m.def("set_rotation", [](const std::string& id, float degrees) {
        GameObject* go = Registry::Get().Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                t->SetRotation(degrees);
            }
        }
    });

    m.def("get_rotation", [](const std::string& id) {
        GameObject* go = Registry::Get().Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                return t->localRotation;
            }
        }
        return 0.0f;
    });

    m.def("set_scale", [](const std::string& id, float x, float y) {
        GameObject* go = Registry::Get().Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                t->SetScale({x, y});
            }
        }
    });

    m.def("get_scale", [](const std::string& id) {
        GameObject* go = Registry::Get().Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                return std::make_tuple(t->localScale.x, t->localScale.y);
            }
        }
        return std::make_tuple(1.0f, 1.0f);
    });
}