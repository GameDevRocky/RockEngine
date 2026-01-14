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
    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->GetRegistry();
    Component* comp = registry->Find<Component>(comp_id);
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
    
    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->GetRegistry();
    Scene* scene = registry->Find<Scene>(scene_id);
    if (scene){
        scene_id = id;
    }
    Notify(); 
}
Scene* GameObject::GetScene(){
    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->GetRegistry();
    Scene* scene = registry->Find<Scene>(scene_id);
    if (!scene) return nullptr;
    return scene;
}

Transform* GameObject::GetTransform(){
    return GetComponent<Transform>();
}

void GameObject::Awake(){
    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->GetRegistry();
    Scene* scene = registry->Find<Scene>(scene_id);

    for (auto& [type, comp_id] : component_ids){
        Component* comp = registry->Find<Component>(comp_id);
        comp->Awake();
    }

}

void GameObject::Update() {
    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->GetRegistry();

    for (auto& [type, comp_id] : component_ids){
        Component* comp = registry->Find<Component>(comp_id);
        comp->Update();

    }
}
void GameObject::FixedUpdate() {
    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->GetRegistry();
    for (auto& [type, comp_id] : component_ids){
        Component* comp = registry->Find<Component>(comp_id);
        comp->FixedUpdate();
    }
}
void GameObject::LateUpdate() {
    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->GetRegistry();
    for (auto& [type, comp_id] : component_ids){
        Component* comp = registry->Find<Component>(comp_id);
        comp->LateUpdate();
    }
}
void GameObject::PostDeserialize() {
    for (auto& comp_id : temp_ids){
        AddComponent(comp_id);
    }
}
