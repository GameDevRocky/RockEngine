#include "engine/utils/pyBindings.hpp"
#include <pybind11/embed.h>
#include <pybind11/stl.h> 
#include "engine/components/Transform.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include <string>

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(engine_api, m) {
    m.doc() = "C++ Core Logic for Python Handles";

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

    // Get Position
    m.def("get_position", [](const std::string& id) {
        GameObject* go = Registry::Get().Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                return std::make_tuple(t->localPosition.x, t->localPosition.y);
            }
        }
        return std::make_tuple(0.0f, 0.0f);
    });

    // Set Rotation
    m.def("set_rotation", [](const std::string& id, float degrees) {
        GameObject* go = Registry::Get().Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                t->SetRotation(degrees);
            }
        }
    });

    // Get Rotation
    m.def("get_rotation", [](const std::string& id) {
        GameObject* go = Registry::Get().Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                return t->localRotation;
            }
        }
        return 0.0f;
    });

    // Set Scale
    m.def("set_scale", [](const std::string& id, float x, float y) {
        GameObject* go = Registry::Get().Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                t->SetScale({x, y});
            }
        }
    });

    // Get Scale
    m.def("get_scale", [](const std::string& id) {
        GameObject* go = Registry::Get().Find<GameObject>(id);
        if (go) {
            if (auto* t = go->GetComponent<Transform>()) {
                return std::make_tuple(t->localScale.x, t->localScale.y);
            }
        }
        return std::make_tuple(1.0f, 1.0f);
    });

} // This closes the PYBIND11_EMBEDDED_MODULE block

namespace engine {
    void RegisterPythonBindings() {
        // This function exists solely to force the linker 
        // to include this file in the final executable.
    }
}