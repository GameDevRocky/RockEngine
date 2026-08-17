#include "engine/bindings/PythonBindings.hpp"

#include "engine/components/ComponentActions.hpp"
#include "engine/components/Component.hpp"
#include "engine/serialization/Registry.hpp"
#include "Engine.hpp"

#include <iostream>

// The bridge an Event uses to fire one call entry. Invocation is routed through
// C++ rather than resolved in Python because ComponentActions is already the one
// registry that knows what a component exposes and how to call it — a second,
// Python-side dispatch table would be the same synchronisation problem the
// Inspector and MCP used to have between them.
void BindComponentActions(pybind11::module_& m) {
    pybind11::module_ actions = m.def_submodule(
        "component_action_module", "Invoking component actions by id");

    // Resolves against the ACTIVE container's registry, so an event fired inside
    // play mode reaches the runtime copy of the component rather than the
    // edit-time original it was wired against.
    actions.def("invoke", [](const std::string& componentId,
                             const std::string& action,
                             const std::string& rawArg) {
        Component* component = registry->Find<Component>(componentId);
        if (!component) {
            std::cerr << "[Event] target component " << componentId
                      << " no longer exists\n";
            return false;
        }
        std::string error;
        if (!ComponentActions::Invoke(component, action, rawArg, &error)) {
            std::cerr << "[Event] " << component->GetTypeName() << "." << action
                      << " failed: " << error << "\n";
            return false;
        }
        return true;
    }, pybind11::arg("component_id"), pybind11::arg("action"),
       pybind11::arg("raw_arg") = std::string());
}
