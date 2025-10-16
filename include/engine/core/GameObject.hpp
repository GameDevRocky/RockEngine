#pragma once
#include <vector>
#include <type_traits>
#include <typeinfo>

class Component; // Forward declare

class GameObject {
public:
    GameObject() = default;
    ~GameObject();

    // Add a new component of type T
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args);

    // Retrieve the first component of type T
    template<typename T>
    T* GetComponent();

private:
    std::vector<Component*> components;
};
