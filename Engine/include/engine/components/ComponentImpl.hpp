#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"

template<typename T>
T* Component::GetComponentInParent() {

    Transform* transform = GetTransform(); 
    if (!transform) return nullptr;

    Transform* parentTransform = transform->GetParent();
    if (!parentTransform) return nullptr;

    GameObject* parentGO = parentTransform->GetGameObject();
    if (!parentGO) return nullptr;
    T* comp = parentGO->template GetComponent<T>();
    if (comp) return comp;
    return parentGO->template GetComponentInParent<T>();
}