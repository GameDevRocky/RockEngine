#include "engine/core/GameObject.hpp"
#include <iostream>
#include "engine/serialization/Registry.hpp"
#include "engine/components/Component.hpp"
#include "engine/components/Transform.hpp"
#include "engine/debug/Console.hpp"
#include "engine/core/Scene.hpp"

void GameObject::AddComponent(Component* comp) {
    if(!comp) return;
    Registry* registry = container->FindSystem<Registry>();
    comp->Attach(this->container);
    comp->SetGameObject(this);
    registry->Register(comp);
    component_ids[comp->GetTypeName()] = comp->GetID(); 
}

void GameObject::Deserialize(const YAML::Node& node) {
    Serializable::Deserialize(node);
    name = node["name"].as<std::string>();
    if (node["component_ids"] && node["component_ids"].IsMap()) {
        for (auto pair : node["component_ids"]) {
            std::string typeName = pair.first.as<std::string>();
            std::string componentId = pair.second.as<std::string>();
            component_ids[typeName] = componentId;
        }
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

void GameObject::Init(){
    registry = container->FindSystem<Registry>();
    for (auto& [type, id] : component_ids){
        Component* comp = registry->Find<Component>(id);
        if (!comp) continue;
        comp->Init();
    }
}

void GameObject::PostInit(){
    registry = container->FindSystem<Registry>();
    for (auto& [type, id] : component_ids){
        Component* comp = registry->Find<Component>(id);
        if (!comp) continue;
        comp->PostInit();
    }
}

void GameObject::Awake(){

    for (auto& [type, id] : component_ids){
        Component* comp = registry->Find<Component>(id);
        if (!comp) {
            std::cout << "GameObject::Awake - Component not found: " << id << " (type: " << type << ")" << std::endl;
            continue;
        }
        std::cout << "GameObject::Awake - Calling Awake on: " << type << " (id: " << id << ")" << std::endl;
        comp->Awake();
    } 
}

void GameObject::Start(){

    for (auto& [type, id] : component_ids){
        Component* comp = registry->Find<Component>(id);
        if (!comp) {
            std::cout << "GameObject::Start - Component not found: " << id << " (type: " << type << ")" << std::endl;
            continue;
        }
        std::cout << "GameObject::Start - Calling Start on: " << type << " (id: " << id << ")" << std::endl;
        comp->Start();
    } 
}

void GameObject::Update() {

    for (auto& [type, id] : component_ids){
        Component* comp = registry->Find<Component>(id);
        if (!comp) continue;
        comp->Update();

    }
}
void GameObject::FixedUpdate() {
    for (auto& [type, id] : component_ids){
        Component* comp = registry->Find<Component>(id);
        if (!comp) continue;
        comp->FixedUpdate();
    }
}
void GameObject::LateUpdate() {
    for (auto& [type, id] : component_ids){
        Component* comp = registry->Find<Component>(id);
        if (!comp) continue;
        comp->LateUpdate();
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

GameObject* GameObject::Copy(Container* container) {
    GameObject* copy = this->Copy();
    copy->Attach(container);
    return copy;
}