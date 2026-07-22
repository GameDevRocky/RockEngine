#include "engine/bindings/PythonBindings.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Animator.hpp"
#include "Engine.hpp"
#include <pybind11/stl.h>

// Scripting surface for the Animator state machine: scripts set parameters that
// drive transitions, and can force/query the current state. Each function takes
// the owning GameObject id and resolves the Animator through the active
// container's Registry (mirrors camera_module).
void BindAnimator(pybind11::module_& m) {
    pybind11::module_ animator_module = m.def_submodule("animator_module", "Animator Bindings");

    animator_module.def("set_float", [](const std::string& id, const std::string& name, float v) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* a = go->GetComponent<Animator>()) a->SetFloat(name, v);
    });
    animator_module.def("set_int", [](const std::string& id, const std::string& name, int v) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* a = go->GetComponent<Animator>()) a->SetInt(name, v);
    });
    animator_module.def("set_bool", [](const std::string& id, const std::string& name, bool v) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* a = go->GetComponent<Animator>()) a->SetBool(name, v);
    });
    animator_module.def("set_trigger", [](const std::string& id, const std::string& name) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* a = go->GetComponent<Animator>()) a->SetTrigger(name);
    });
    animator_module.def("reset_trigger", [](const std::string& id, const std::string& name) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* a = go->GetComponent<Animator>()) a->ResetTrigger(name);
    });

    animator_module.def("get_float", [](const std::string& id, const std::string& name) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* a = go->GetComponent<Animator>()) return a->GetFloat(name);
        return 0.0f;
    });
    animator_module.def("get_int", [](const std::string& id, const std::string& name) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* a = go->GetComponent<Animator>()) return a->GetInt(name);
        return 0;
    });
    animator_module.def("get_bool", [](const std::string& id, const std::string& name) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* a = go->GetComponent<Animator>()) return a->GetBool(name);
        return false;
    });

    animator_module.def("get_current_state", [](const std::string& id) -> std::string {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* a = go->GetComponent<Animator>()) return a->GetCurrentState();
        return {};
    });
    animator_module.def("play", [](const std::string& id, const std::string& stateName) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) if (auto* a = go->GetComponent<Animator>()) a->Play(stateName);
    });
}
