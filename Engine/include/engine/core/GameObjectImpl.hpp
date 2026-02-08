#pragma once
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"
#include "engine/utils/EngineUtils.hpp"

template<typename T>
T* GameObject::GetComponentInParent() {
    return GetTransform()->GetComponentInParent<T>();
}


template<typename T>
T* GameObject::RequireComponent() {
    T* comp = GetComponent<T>();
    if (comp) return comp;
    comp = new T();
    comp->SetID(EngineUtils::GenerateUUID());
    
    AddComponent(comp);
    return comp;
}