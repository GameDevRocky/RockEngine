#include "engine/bindings/PythonBindings.hpp"

#include "engine/serialization/Registry.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/Scene.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/Component.hpp"
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

    gameobject_module.def("set_name", [](const std::string& id, std::string& val) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id); 
        if (go) {
            go->SetName(val);
        }
    });

    gameobject_module.def("get_name", [](const std::string& id) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id); 
        if (go) {
            return go->GetName();
        }
        return std::string("");
    });

    // Add a component to a GameObject by type name.
    // Uses SerializableFactory so any registered component type is supported.
    // Returns the new component's ID, or empty string if the GO doesn't exist,
    // the type is unrecognised, or the component already exists.
    gameobject_module.def("add_component", [](const std::string& go_id, const std::string& type_name) -> std::string {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(go_id);
        if (!go) return {};

        if (go->HasComponentByName(type_name)) return {};

        Serializable* raw = SerializableFactory::Create(type_name);
        Component* comp = dynamic_cast<Component*>(raw);
        if (!comp) {
            delete raw;
            return {};
        }

        std::string comp_id = comp->GetID();
        go->AddComponent(comp);
        return comp_id;
    });

    // Instantiate a new GameObject in the same scene as the caller.
    // Returns the new GameObject's ID, or empty string on failure.
    gameobject_module.def("instantiate", [](const std::string& caller_id, const std::string& name) -> std::string {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();

        GameObject* caller = registry->Find<GameObject>(caller_id);
        if (!caller) return {};

        Scene* scene = caller->GetScene();
        if (!scene) return {};

        auto* obj = new GameObject();
        obj->SetName(name);

        Transform* t = new Transform();
       

        scene->AddGameObject(obj);
        obj->AddComponent(t);
        return obj->GetID();
    });

}