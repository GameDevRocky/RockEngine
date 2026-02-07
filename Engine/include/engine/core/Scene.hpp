#pragma once
#include <vector>
#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"
#include "engine/serialization/Serializable.hpp"
#include "engine/core/RuntimeObject.hpp"

class GameObject;
class Component;
class Registry;

class Scene : public RuntimeObject{
public:

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;

    void Init() override;
    void PostInit() override;
    void Awake() override;
    void Start() override;

    void Update() override;
    void FixedUpdate();
    void LateUpdate();
    
    std::string GetTypeName() override { return "Scene"; }
    
    void AddGameObject(GameObject* obj);
    void RemoveGameObject(GameObject* obj){};
    
    std::vector<GameObject*> GetRootObjects();
    std::vector<GameObject*> GetAllGameObjects();
    
    const std::string& GetName() const { return name; }
    void SetName(const std::string& newName) { name = newName; Notify(); }
    
    Scene* Copy() override;
    Scene* Copy(Container* container) override;

    Scene() = default;
    ~Scene() = default;

private:
    
    std::string name;
    std::vector<std::string> rootobject_ids;
    Registry* registry = nullptr;


};
