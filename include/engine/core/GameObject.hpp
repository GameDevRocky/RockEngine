#pragma once
#include <vector>
#include <type_traits>
#include <typeinfo>
#include <string>
#include "engine/serialization/Serializable.hpp"
#include "engine/serialization/Registry.hpp"

class Component;
class Transform;
class GameObject : public Serializable {

public:
    GameObject() = default;
    ~GameObject() =default;

      template<typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
        T* comp = new T(std::forward<Args>(args)...);
        comp->gameobject = this;
        components.push_back(comp);
        Registry::Get().Register(comp);
        return comp;
    }

    template<typename T>
    T* GetComponent() {
        for (auto* c : components)
            if (auto* t = dynamic_cast<T*>(c))
                return t;
        return nullptr;
    }

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void PostDeserialize() override;
    std::string GetTypeName() override {return "GameObject";}
    void Link() override{};
    Transform* transform = nullptr;

private:
    std::string name;
    std::vector<Component*> components;
    std::vector<std::string> component_ids;
};
