#include "engine/bindings/PythonBindings.hpp"

#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"

void BindComponent(pybind11::module_& m) {
    // COMPONENT API

    m.def("set_enabled", [](const std::string& id, bool val) {
        Component* comp = Registry::Get().Find<Component>(id); 
        if (comp) {
            comp->SetEnabled(val);
        }
    });

    m.def("get_enabled", [](const std::string& id) {
        Component* comp = Registry::Get().Find<Component>(id); 
        if (comp) {
            return comp->GetEnabled();
        }
        return false;
    });

}