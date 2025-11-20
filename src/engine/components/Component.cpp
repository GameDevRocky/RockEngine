#include "engine/components/Component.hpp"
#include "engine/serialization/Registry.hpp"

void Component::Deserialize(const YAML::Node& node){
    Serializable::Deserialize(node);
    gameobject_id = node["gameobject"].as<std::string>();

}


GameObject* Component::GetGameObject(){
    if (gameobject_id.empty()) return nullptr;
    Serializable* s = Registry::Get().Find(gameobject_id);
    GameObject* gameobject = dynamic_cast<GameObject*>(s);
    if (!gameobject) return nullptr;
    return gameobject;
}

void Component::SetEnabled(bool e) {
    if (e == enabled) return;
    enabled = e;
    if (enabled) OnEnabled();
    else OnDisabled();
}
