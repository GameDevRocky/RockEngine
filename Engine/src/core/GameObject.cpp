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
    registry->Register(comp);
    component_ids[comp->GetTypeName()] = comp->GetID(); 
    comp->SetGameObject(this);
    this->Notify(GameObject::ADD_COMPONENT_EVENT);
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

void GameObject::SetScene(Scene* scene){
    if (!scene) return;
    if (scene_id == scene->GetID()) return;
    scene_id = scene->GetID();
    Notify(GameObject::SCENE_CHANGED_EVENT); 
}

Scene* GameObject::GetScene(){
    Registry* registry = container->FindSystem<Registry>();
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

void GameObject::SetActive(bool active){
    bool notify = false;
    if (this->active != active){
        notify = true;
    }
    this->active = active;
    if (notify) Notify(GameObject::ACTIVE_CHANGED_EVENT); 
}
void GameObject::SetName(std::string& name){
    bool notify = false;
    if (this->name != name){
        notify = true;
    }
    this->name = name;
    if (notify) Notify(GameObject::NAME_CHANGED_EVENT); 
}

GameObject* GameObject::Copy(){
    GameObject* copy = new GameObject();
    copy->id = id;
    copy->name = name;
    copy->subscribers = subscribers;
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