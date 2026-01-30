#pragma once
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"

// Template implementation requiring full Transform definition
template<typename T>
T* GameObject::GetComponentInParent() {
    return GetTransform()->GetComponentInParent<T>();
}
