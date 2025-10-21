#include "engine/core/GameObject.hpp"
#include <iostream>
#include "engine/serialization/Registry.hpp"
#include "engine/components/Component.hpp"
#include "engine/components/Transform.hpp"

YAML::Node GameObject::Serialize() {
    YAML::Node node;
    node["id"] = id;
    node["name"] = name;

    YAML::Node compNode;
    for (auto* comp : components)
        compNode.push_back(comp->GetID());
    node["components"] = compNode;

    return node;
}

void GameObject::Deserialize(const YAML::Node& node) {
    id = node["id"].as<std::string>();
    name = node["name"].as<std::string>();

    Registry::Get().Register(this);

    if (node["components"]) {
        for (auto& compIdNode : node["components"]) {
            std::string cid = compIdNode.as<std::string>();

            Registry::Get().DeferLink(cid, [this](Serializable* obj) {
                if (auto* comp = dynamic_cast<Component*>(obj)) {
                    this->components.push_back(comp);
                } else {
                    std::cerr << "[DeferLink] Invalid component type for ID.\n";
                }
            });
        }
    }
}


void GameObject::PostDeserialize() {
    transform = GetComponent<Transform>();
    transform->Translate(glm::vec2(5,9));
}
