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
    Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
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
    Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
    Scene* scene = registry->Find<Scene>(scene_id);
    if (scene){
        scene_id = id;
    }
    Notify(); 
}
Scene* GameObject::GetScene(){
    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
    Scene* scene = registry->Find<Scene>(scene_id);
    if (!scene) return nullptr;
    return scene;
}

Transform* GameObject::GetTransform(){
    return GetComponent<Transform>();
}

void GameObject::Awake(){

    Registry* registry = container->FindSystem<Registry>();
    Scene* scene = registry->Find<Scene>(scene_id);

    for (auto& [type, comp_id] : component_ids){
        Component* comp = registry->Find<Component>(comp_id);
        if (!comp) continue;
        comp->Awake();
    }

}

void GameObject::Update() {
    Engine* engine = Engine::Get();
    Registry* registry = container->FindSystem<Registry>();

    for (auto& [type, comp_id] : component_ids){
        Component* comp = registry->Find<Component>(comp_id);
        if (!comp) continue;
        comp->Update();

    }
}
void GameObject::FixedUpdate() {
    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
    for (auto& [type, comp_id] : component_ids){
        Component* comp = registry->Find<Component>(comp_id);
        if (!comp) continue;
        comp->FixedUpdate();
    }
}
void GameObject::LateUpdate() {
    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
    for (auto& [type, comp_id] : component_ids){
        Component* comp = registry->Find<Component>(comp_id);
        if (!comp) continue;
        comp->LateUpdate();
    }
}
void GameObject::PostDeserialize() {
    for (auto& comp_id : temp_ids){
        AddComponent(comp_id);
    }
}

GameObject* GameObject::Copy(){
    GameObject* copy = new GameObject();
    copy->id = id;
    copy->name = name;
    copy->active = active;
    copy->component_ids = component_ids;
    copy->transform_id = transform_id;
    copy->temp_ids = temp_ids;
    copy->scene_id = scene_id;
    return copy;
}