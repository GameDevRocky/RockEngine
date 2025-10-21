#include "engine/components/Component.hpp"
#include "engine/serialization/Registry.hpp"

void Component::Deserialize(const YAML::Node& node){
    Serializable::Deserialize(node);
    if (node["gameobject"]){

        std::string gameobject_id = node["gameobject"].as<std::string>();
        Registry::Get().DeferLink(gameobject_id, [this](Serializable* obj) {
            if (auto* g_obj = dynamic_cast<GameObject*>(obj)) {
                this->gameobject = g_obj;
            } else {
                std::cerr << "[DeferLink] Invalid component type for ID.\n";
            }
        });
    }
}
