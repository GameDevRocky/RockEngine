#include "engine/core/GameObject.hpp"
#include <iostream>
// AddComponent returns reference
template<typename T, typename... Args>
T* GameObject::AddComponent(Args&&... args) {
    static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
    T* comp = new T(std::forward<Args>(args)...);
    comp->gameObject = this;
    components.push_back(comp);
    return comp;
}

// GetComponent returns pointer
template<typename T>
T* GameObject::GetComponent() {
    for (auto* c : components)
        if (auto* t = dynamic_cast<T*>(c))
            return t;
    return nullptr;
}

YAML::Node GameObject::Serialize(){
    YAML::Node node;
    return node;
}

void GameObject::Deserialize(const YAML::Node &node){
    Serializable::Deserialize(node);
    name = node["name"].as<std::string>();
    std::cout << name << ", ";
}
