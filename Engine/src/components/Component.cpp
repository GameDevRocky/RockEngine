#include "engine/components/Component.hpp"
#include "engine/serialization/Registry.hpp"
#include "Engine.hpp"

void Component::Deserialize(const YAML::Node& node){
    Serializable::Deserialize(node);
    gameobject_id = node["gameobject"].as<std::string>();

}


GameObject* Component::GetGameObject(){
    if (gameobject_id.empty()) return nullptr;
    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
    GameObject* gameobject = registry->Find<GameObject>(gameobject_id);
    if (!gameobject) return nullptr;
    return gameobject;
}

void Component::SetEnabled(bool e) {
    if (e == enabled) return;
    enabled = e;
    if (enabled) OnEnabled();
    else OnDisabled();
}
