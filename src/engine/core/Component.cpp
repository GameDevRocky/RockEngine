#include "engine/core/Component.hpp"
#include "engine/serialization/Registry.hpp"

void Component::Deserialize(YAML::Node node){
    Serializable::Deserialize(node);
    if (node["gameobject"]){

        std::string gameobject_id = node["gameobject"].as<std::string>();
        Registry::Get().DeferLink(gameobject_id, [this](Serializable* obj) {
            if (auto* g_obj = dynamic_cast<GameObject*>(obj)) {
                this->gameobject = g_obj;
                
                std::cout << "Linked Component (via Defer): " << g_obj->GetTypeName()
                << " (ID: " << g_obj->GetID() << ")\n";
            } else {
                std::cerr << "[DeferLink] Invalid component type for ID.\n";
            }
        });
    }
}
