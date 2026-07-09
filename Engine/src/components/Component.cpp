#include "engine/components/Component.hpp"
#include "engine/serialization/Registry.hpp"
#include "Engine.hpp"
#include <unordered_set>

bool Component::IsSingleton(const std::string& typeName) {
    static const std::unordered_set<std::string> kSingletons = {
        "Transform", "RigidBody"
    };
    return kSingletons.count(typeName) > 0;
}

void Component::Deserialize(const YAML::Node& node){
    Serializable::Deserialize(node);
    gameobject_id = node["gameobject"].as<std::string>();
    enabled = node["enabled"].as<bool>();

}

void Component::SetGameObject(GameObject* obj){
    if (!obj){
        std::cout << "Unable to set " << this->GetTypeName() << " Component to GameObject" << std::endl;
        return;
    }
    this->gameobject_id = obj->GetID();
    Notify();
}

GameObject* Component::GetGameObject(){
    if (gameobject_id.empty()) return nullptr;
    Registry* registry = container->FindSystem<Registry>();
    GameObject* gameobject = registry->Find<GameObject>(gameobject_id);
    if (!gameobject) return nullptr;
    return gameobject;
}

void Component::SetEnabled(bool e) {
    if (e == enabled) return;
    enabled = e;
    if (enabled){
        OnEnabled();
        Notify(Component::ENABLED_CHANGED_EVENT, enabled);
    }
    else{ 
        OnDisabled();
        Notify(Component::ENABLED_CHANGED_EVENT, enabled);
    }
}
void Component::Accept(IVisitor* v) {
    
    v->Visit(this); 
}
Component* Component::Copy(Container* container) {
    auto* copiedSerializable = static_cast<Serializable*>(this)->Copy();
    auto* copiedComponent = dynamic_cast<Component*>(copiedSerializable);
    if (copiedComponent)
        copiedComponent->Attach(container);
    return copiedComponent;
}
