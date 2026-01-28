#include "engine/bindings/PythonBindings.hpp"

#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"
#include "Engine.hpp"

void BindGameObject(pybind11::module_& m) {
    pybind11::module_ gameobject_module = m.def_submodule("gameobject_module", "GameObject Bindings");

    gameobject_module.def("set_active", [](const std::string& id, bool val) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id); 
        if (go) {
            go->SetActive(val);
        }
    });

    gameobject_module.def("get_active", [](const std::string& id) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id); 
        if (go) {
            return go->GetActive();
        }
        return false;
    });

}