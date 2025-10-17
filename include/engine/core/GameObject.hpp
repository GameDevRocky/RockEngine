#pragma once
#include <vector>
#include <type_traits>
#include <typeinfo>
#include <string>
class Component; // Forward declare

class GameObject {
public:
GameObject() = default;
~GameObject();

template<typename T, typename... Args>
T* AddComponent(Args&&... args);

template<typename T>
T* GetComponent();

private:
int id;

std::vector<Component*> components;
std::string name;



};
