#include "engine/bindings/PythonBindings.hpp"

#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"
#include "Engine.hpp"

void BindComponent(pybind11::module_& m) {
    pybind11::module_ base_component = m.def_submodule("base_component_module", "Base Component Bindings");
    

    base_component.def("set_enabled", [](const std::string& id, bool val) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        Component* comp = registry->Find<Component>(id); 
        if (comp) {
            comp->SetEnabled(val);
        }
    });

    base_component.def("get_enabled", [](const std::string& id) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        Component* comp = registry->Find<Component>(id); 
        if (comp) {
            return comp->GetEnabled();
        }
        return false;
    });

}