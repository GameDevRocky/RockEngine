#include "engine/core/GameObject.hpp"
#include <iostream>
#include "engine/serialization/Registry.hpp"
#include "engine/components/Component.hpp"
#include "engine/components/Transform.hpp"
#include "engine/debug/Console.hpp"
#include "engine/core/Scene.hpp"

YAML::Node GameObject::Serialize() {
    YAML::Node node;
    node["id"] = id;
    node["name"] = name;

    YAML::Node compNode;
    for (auto& [type, id] : component_ids)
        compNode.push_back(id);
    node["components"] = compNode;

    return node;
}

void GameObject::AddComponent(const std::string& comp_id) {
        Serializable* s = Registry::Get().Find(comp_id);
        Component* comp = dynamic_cast<Component*>(s);
        if (!comp) return;
        component_ids[comp->GetTypeName()] = comp_id;       
}

void GameObject::Deserialize(const YAML::Node& node) {
    Serializable::Deserialize(node);
    name = node["name"].as<std::string>();
    for (auto& id_node : node["component_ids"]) {
        std::string comp_id = id_node.as<std::string>();
        temp_ids.push_back(comp_id);
    }
}

void GameObject::SetScene(const std::string& id){
    Serializable* s = Registry::Get().Find(id);
    Scene* scene = dynamic_cast<Scene*>(s);
    if (scene){
        scene_id = id;
    }
    Notify(); 
}
Scene* GameObject::GetScene(){
    Serializable* s = Registry::Get().Find(scene_id);
    Scene* scene = dynamic_cast<Scene*>(s);
    if (!scene) return nullptr;
    return scene;
}

Transform* GameObject::GetTransform(){
    return GetComponent<Transform>();
}

void GameObject::PostDeserialize() {
    for (auto& comp_id : temp_ids){
        AddComponent(comp_id);
    }
}
