#include "engine/core/GameObject.hpp"
#include <iostream>
#include "engine/serialization/Registry.hpp"
#include "engine/components/Component.hpp"
#include "engine/components/Transform.hpp"
#include "engine/debug/Console.hpp"
#include "engine/core/Scene.hpp"
#include "engine/utils/IVisitor.hpp"

void GameObject::Accept(IVisitor* v) {
    v->Visit(this);
}

void GameObject::AddComponent(Component* comp) {
    if(!comp) return;
    // Single-instance components (Transform, RigidBody) may only be attached once.
    // comp is freshly created and not yet registered/attached, so deleting it here
    // is safe and avoids leaking on a rejected duplicate.
    if (Component::IsSingleton(comp->GetTypeName()) && HasComponentByName(comp->GetTypeName())) {
        delete comp;
        return;
    }
    Registry* registry = container->FindSystem<Registry>();
    comp->Attach(this->container);
    registry->Register(comp);
    component_ids.push_back(comp->GetID());
    comp->SetGameObject(this);
    comp->Init();
    comp->PostInit();

    if (container->GetMode() == Container::Mode::Runtime){
        comp->Awake();
        comp->Start();
    }
    this->Notify(GameObject::ADD_COMPONENT_EVENT);
}

void GameObject::RemoveComponent(Component* comp) {
    if (!comp) return;

    // Drop the id first so GetAllComponents()/GetComponent() stop returning it
    // immediately; the component object itself is torn down on the next flush.
    const std::string compId = comp->GetID();
    auto it = std::find(component_ids.begin(), component_ids.end(), compId);
    if (it != component_ids.end()) component_ids.erase(it);

    // Deferred Shutdown()+delete via the registry — same path GameObject deletion
    // uses, so component-specific cleanup (physics bodies, etc.) still runs safely.
    Registry* registry = container->FindSystem<Registry>();
    if (registry) registry->Destroy(comp);
    comp->Shutdown();

    this->Notify(GameObject::REMOVE_COMPONENT_EVENT, compId);
}

void GameObject::Deserialize(const YAML::Node& node) {
    Serializable::Deserialize(node);
    name = node["name"].as<std::string>();
    if (node["tag"]) {
        tag = node["tag"].as<std::string>("Untagged");
        if (tag.empty()) tag = "Untagged";
    };
    if (node["component_ids"]) {
        const YAML::Node& cids = node["component_ids"];
        if (cids.IsSequence()) {
            // Current format: an ordered list of component ids.
            for (const auto& n : cids) component_ids.push_back(n.as<std::string>());
        } else if (cids.IsMap()) {
            // Legacy format: map keyed by type name → keep the ids in file order.
            for (const auto& pair : cids) component_ids.push_back(pair.second.as<std::string>());
        }
    }
}

bool GameObject::HasComponentByName(const std::string& type_name) const {
    if (!container) return false;
    Registry* registry = container->FindSystem<Registry>();
    if (!registry) return false;
    for (const auto& id : component_ids) {
        Component* comp = registry->Find<Component>(id);
        if (comp && comp->GetTypeName() == type_name) return true;
    }
    return false;
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

std::vector<Component*> GameObject::GetAllComponents(){
    std::vector<Component*> result;
    result.reserve(component_ids.size());
    Registry* registry = container->FindSystem<Registry>();
    for (auto& id : component_ids ){
        auto* comp = registry->Find<Component>(id);
        if (!comp) continue;
        result.push_back(comp);
    }
    return result;
}

void GameObject::Init(){
    registry = container->FindSystem<Registry>();
    for (auto& id : component_ids){
        Component* comp = registry->Find<Component>(id);
        if (!comp) continue;
        comp->Init();
    }
}

void GameObject::PostInit(){
    registry = container->FindSystem<Registry>();
    for (auto& id : component_ids){
        Component* comp = registry->Find<Component>(id);
        if (!comp) continue;
        comp->PostInit();
    }
}

void GameObject::Awake(){

    for (auto& id : component_ids){
        Component* comp = registry->Find<Component>(id);
        if (!comp) {
            continue;
        }
        if (!comp->GetEnabled()) continue;
        comp->Awake();
    } 
}

void GameObject::Start(){

    for (auto& id : component_ids){
        Component* comp = registry->Find<Component>(id);
        if (!comp) {
            continue;
        }
        if (!comp->GetEnabled()) continue;
        comp->Start();
    } 
}

void GameObject::Update() {

    for (auto& id : component_ids){
        Component* comp = registry->Find<Component>(id);
        if (!comp) continue;
        
        if (!comp->GetEnabled()) continue;
        comp->Update();

    }
}
void GameObject::FixedUpdate() {
    for (auto& id : component_ids){
        Component* comp = registry->Find<Component>(id);
        if (!comp) continue;
        if (!comp->GetEnabled()) continue;
        comp->FixedUpdate();
    }
}

void GameObject::LateUpdate() {
    for (auto& id : component_ids) {
        Component* comp = registry->Find<Component>(id);
        if (!comp) continue;
        if (!comp->GetEnabled()) continue;
        comp->LateUpdate();
    }
}

void GameObject::OnCollisionEnter(GameObject* other){
    for (auto& id : component_ids){
        Component* comp = registry->Find<Component>(id);
        if (!comp) continue;
        if (!comp->GetEnabled()) continue;
        comp->OnCollisionEnter(other);
    }
}
void GameObject::OnCollisionExit(GameObject* other) {
    for (auto& id : component_ids) {
        Component* comp = registry->Find<Component>(id);
        if (!comp) continue;
        if (!comp->GetEnabled()) continue;
        comp->OnCollisionExit(other);
    }
}

void GameObject::OnTriggerEnter(GameObject* other) {
    for (auto& id : component_ids) {
        Component* comp = registry->Find<Component>(id);
        if (!comp) continue;
        if (!comp->GetEnabled()) continue;
        comp->OnTriggerEnter(other);
    }
}
void GameObject::OnTriggerExit(GameObject* other) {
    for (auto& id : component_ids) {
        Component* comp = registry->Find<Component>(id);
        if (!comp) continue;
        if (!comp->GetEnabled()) continue;
        comp->OnTriggerExit(other);
    }
}

void GameObject::SetActive(bool active){
    bool notify = false;
    if (this->active != active){
        notify = true;
    }
    this->active = active;
    for (auto& id : component_ids) {
        Component* comp = registry->Find<Component>(id);
        if (!comp) continue;
        comp->SetEnabled(active);
    }

    Transform* t = GetTransform();
    if (t) {
        for (Transform* child : t->GetChildren()) {
            if (!child) continue;
            GameObject* childGo = child->GetGameObject();
            if (childGo) childGo->SetActive(active);
        }
    }

    if (notify) Notify(GameObject::ACTIVE_CHANGED_EVENT, active); 
}
void GameObject::SetTag(const std::string& newTag){
    if (this->tag == newTag) return;
    this->tag = newTag;
    Notify(GameObject::TAG_CHANGED_EVENT, newTag);
}
void GameObject::SetName(const std::string& name){
    bool notify = false;
    if (this->name != name){
        notify = true;
    }
    this->name = name;
    if (notify) Notify(GameObject::NAME_CHANGED_EVENT, name); 
}
void GameObject::Shutdown(){
    // Bottom-up: shut down children before self so hierarchy is always
    // cleaned leaf-first regardless of how Shutdown() is triggered.
    if (Transform* t = GetTransform()) {
        for (Transform* child : t->GetChildren()) {
            if (GameObject* childGo = child->GetGameObject())
                childGo->Shutdown();
        }
    }

    auto ids = component_ids;
    for (auto& id : ids) {
        Component* comp = registry->Find<Component>(id);
        if (!comp) continue;
        comp->Shutdown();
    }
    RuntimeObject::Shutdown();
}


GameObject* GameObject::Copy(){
    GameObject* copy = new GameObject();
    copy->id = id;
    copy->name = name;
    copy->subscribers = subscribers;
    copy->active = active;
    copy->tag = tag;
    copy->component_ids = component_ids;
    copy->transform_id = transform_id;
    copy->scene_id = scene_id;
    return copy;
}

GameObject* GameObject::Copy(Container* container) {
    GameObject* copy = this->Copy();
    copy->Attach(container);
    return copy;
}