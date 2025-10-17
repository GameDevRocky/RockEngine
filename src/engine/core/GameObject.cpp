#include "engine/core/GameObject.hpp"
#include <iostream>
#include "engine/serialization/Registry.hpp"
#include "engine/core/Component.hpp"

// ------------------
// Template Methods
// ------------------

template<typename T, typename... Args>
T* GameObject::AddComponent(Args&&... args) {
    static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
    T* comp = new T(std::forward<Args>(args)...);
    comp->gameObject = this;
    components.push_back(comp);
    return comp;
}

template<typename T>
T* GameObject::GetComponent() {
    for (auto* c : components)
        if (auto* t = dynamic_cast<T*>(c))
            return t;
    return nullptr;
}

// ------------------
// Serialization
// ------------------
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

                    std::cout << "Linked Component (via Defer): " << comp->GetTypeName()
                              << " (ID: " << comp->GetID() << ")\n";
                } else {
                    std::cerr << "[DeferLink] Invalid component type for ID.\n";
                }
            });
        }
    }
}


void GameObject::Link() {
}
