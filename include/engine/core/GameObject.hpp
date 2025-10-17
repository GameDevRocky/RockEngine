#pragma once
#include <vector>
#include <type_traits>
#include <typeinfo>
#include <string>
#include "engine/serialization/Serializable.hpp"
class Component; // Forward declare

class GameObject : public Serializable {

public:
GameObject() = default;
~GameObject() =default;

template<typename T, typename... Args>
T* AddComponent(Args&&... args);

template<typename T>
T* GetComponent();

YAML::Node Serialize() override;
void Deserialize(const YAML::Node& node) override;
std::string GetTypeName() override {return "GameObject";}

private:
int id;
std::string name;
std::vector<Component*> components;



};
