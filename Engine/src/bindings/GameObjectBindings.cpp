#include "engine/bindings/PythonBindings.hpp"

#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"
#include "Engine.hpp"

void BindGameObject(pybind11::module_& m) {
    // GAMEOBJECT API

    m.def("set_active", [](const std::string& id, bool val) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->GetRegistry();
        GameObject* go = registry->Find<GameObject>(id); 
        if (go) {
            go->SetActive(val);
        }
    });

    m.def("get_active", [](const std::string& id) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->GetRegistry();
        GameObject* go = registry->Find<GameObject>(id); 
        if (go) {
            return go->GetActive();
        }
        return false;
    });

}