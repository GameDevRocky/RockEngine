#include "engine/bindings/PythonBindings.hpp"

#include "engine/serialization/Registry.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/TagManager.hpp"
#include "engine/debug/Console.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/Component.hpp"

void BindGameObject(pybind11::module_& m) {
    pybind11::module_ gameobject_module = m.def_submodule("gameobject_module", "GameObject Bindings");

    gameobject_module.def("get_tag", [](const std::string& id) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) return go->GetTag();
        return std::string("");
    });

    gameobject_module.def("set_tag", [](const std::string& id, const std::string& tag) {
        GameObject* go = registry->Find<GameObject>(id);
        if (!go) return;
        if (!tagManager || !tagManager->HasTag(tag)) {
            Console::Alert("Tag '" + tag + "' is not registered in TagManager.");
            return;
        }
        go->SetTag(tag);
    });

    gameobject_module.def("set_active", [](const std::string& id, bool val) {
        GameObject* go = registry->Find<GameObject>(id); 
        if (go) {
            go->SetActive(val);
        }
    });

    gameobject_module.def("get_active", [](const std::string& id) {
        GameObject* go = registry->Find<GameObject>(id); 
        if (go) {
            return go->GetActive();
        }
        return false;
    });

    gameobject_module.def("set_name", [](const std::string& id, std::string& val) {
        GameObject* go = registry->Find<GameObject>(id); 
        if (go) {
            go->SetName(val);
        }
    });

    gameobject_module.def("get_name", [](const std::string& id) {
        GameObject* go = registry->Find<GameObject>(id); 
        if (go) {
            return go->GetName();
        }
        return std::string("");
    });

    gameobject_module.def("add_component", [](const std::string& go_id, const std::string& type_name) -> std::string {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (!go) return {};
        // Only single-instance components are blocked from duplication.
        if (Component::IsSingleton(type_name) && go->HasComponentByName(type_name)) return {};
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

    // Every component of a given type, by component id. get_component() can only
    // ever name one instance per type; anything genuinely multi-instance (joints,
    // colliders) needs the individual ids to address a specific one.
    gameobject_module.def("get_component_ids", [](const std::string& go_id, const std::string& type_name) {
        std::vector<std::string> ids;
        GameObject* go = registry->Find<GameObject>(go_id);
        if (!go) return ids;
        for (Component* comp : go->GetAllComponents()) {
            if (comp && comp->GetTypeName() == type_name) ids.push_back(comp->GetID());
        }
        return ids;
    });

    gameobject_module.def("instantiate", [](const std::string& caller_id, const std::string& name) -> std::string {
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

    gameobject_module.def("duplicate", [](const std::string& target_id) -> std::string {
        GameObject* source = registry->Find<GameObject>(target_id);
        if (!source) return {};
        Scene* scene = source->GetScene();
        if (!scene) return {};
        GameObject* clone = scene->DuplicateGameObject(source);
        return clone ? clone->GetID() : std::string{};
    });

    gameobject_module.def("shut_down", [](const std::string& id){
        GameObject* obj = registry->Find<GameObject>(id);
        if (!obj) return;
        registry->Destroy(obj);
    });

    gameobject_module.def("get_scene_id", [](const std::string& id) -> std::string {
        GameObject* go = registry->Find<GameObject>(id);
        if (!go) return {};
        Scene* scene = go->GetScene();
        return scene ? scene->GetID() : std::string{};
    });

}