#pragma once
#include <vector>
#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"
#include "engine/serialization/Serializable.hpp"

class GameObject;

class Scene : public Serializable {
public:
    Scene() = default;
    ~Scene() = default;

    void Init();
    void Update();
    void FixedUpdate();
    void LateUpdate();
    void Shutdown();

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    std::string GetTypeName() override { return "Scene"; }

    // Scene membership
    void AddGameObject(GameObject* obj);
    void RemoveGameObject(const std::string& obj_id){};

    // Root membership
    void AddRootObject(const std::string& obj_id);
    void RemoveRootObject(const std::string& obj_id);

    // Query
    std::vector<GameObject*> GetRootObjects();
    std::vector<GameObject*> GetAllGameObjects();

    // Scene properties
    const std::string& GetName() const { return name; }
    void SetName(const std::string& newName) { name = newName; Notify(); }

private:
    std::string name;

    // Only ID references — NOT ownership
    std::vector<std::string> root_object_ids;
    std::vector<std::string> gameobject_ids;
};
