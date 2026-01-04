#pragma once
#include <vector>
#include <type_traits>
#include <typeinfo>
#include <string>
#include "engine/serialization/Serializable.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/core/Scene.hpp"
#include <iostream>

class Component;
class Transform;
class GameObject : public Serializable {

public:
    std::string name;
    GameObject() = default;
    ~GameObject() =default;

    void AddComponent(const std::string& comp_id);

    template<typename T>
    T* GetComponent() {
        std::string type = std::string(EngineUtils::TypeName<T>());
        auto it = component_ids.find(type);
        if (it == component_ids.end())
            return nullptr;

        const std::string& comp_id = it->second;
        T* comp = Registry::Find<T>(comp_id);
        return comp;
    }


    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void PostDeserialize() override;
    Transform* GetTransform();
    std::string GetTypeName() override {return "GameObject";}
    std::string GetName() {return name;}
    void SetScene(const std::string& id);
    Scene* GetScene();
    void Awake();
    void Update();
    void FixedUpdate();
    void LateUpdate();

    
    private:
    std::map<std::string, std::string> component_ids;
    std::string transform_id;
    std::vector<std::string> temp_ids;
    std::string scene_id;
};
