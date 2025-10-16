#include "engine/core/GameObject.hpp"

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
