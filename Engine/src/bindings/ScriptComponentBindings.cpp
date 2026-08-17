#include "engine/bindings/PythonBindings.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/ScriptComponent.hpp"
#include "Engine.hpp"
#include <pybind11/stl.h>

// Resolving one script's reference to another script.
//
// A `manager: GameManager` field stores the target's GameObject id, and this is
// what turns that id back into the live Python instance running on it. It is
// resolved on every access rather than captured once (see ScriptRef in
// Domain/lib/api/components/script_ref.py) because a hot-reload replaces the
// instance object wholesale — anything holding the old one keeps talking to a
// script that is no longer attached to anything.
void BindScriptComponent(pybind11::module_& m) {
    pybind11::module_ script_module = m.def_submodule("script_module", "ScriptComponent Bindings");

    // The live Python instance of `class_name` running on `gameobject_id`, or
    // None when the object is gone, carries no such script, or the script failed
    // to load (a missing .py leaves the component with no instance at all).
    script_module.def("get_script_instance",
                      [](const std::string& gameobjectId,
                         const std::string& className) -> pybind11::object {
        GameObject* go = registry->Find<GameObject>(gameobjectId);
        if (!go) return pybind11::none();
        for (Component* component : go->GetAllComponents()) {
            auto* script = dynamic_cast<ScriptComponent*>(component);
            if (!script || script->GetScriptClassName() != className) continue;
            void* handle = script->GetScriptInstanceHandle();
            if (!handle) return pybind11::none();
            return pybind11::reinterpret_borrow<pybind11::object>(
                reinterpret_cast<PyObject*>(handle));
        }
        return pybind11::none();
    }, pybind11::arg("gameobject_id"), pybind11::arg("class_name"));

    // Which script classes an object runs. Lets Python answer "does this object
    // have that script" without constructing a reference to it first.
    script_module.def("get_script_class_names",
                      [](const std::string& gameobjectId) {
        std::vector<std::string> names;
        GameObject* go = registry->Find<GameObject>(gameobjectId);
        if (!go) return names;
        for (Component* component : go->GetAllComponents())
            if (auto* script = dynamic_cast<ScriptComponent*>(component))
                if (!script->GetScriptClassName().empty())
                    names.push_back(script->GetScriptClassName());
        return names;
    }, pybind11::arg("gameobject_id"));
}
