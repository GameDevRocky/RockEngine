#pragma once
#include <vector>
#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"
#include "engine/serialization/Serializable.hpp"
#include "engine/core/RuntimeObject.hpp"

class GameObject;

class Scene : public Serializable, public RuntimeObject{
public:
    Scene() = default;
    ~Scene() = default;

    
    void Init();
    void PostInit();
    
    void Awake();
    void Update();
    void FixedUpdate();
    void LateUpdate();

    void OnEnterPlayMode() override;
    void OnExitPlayMode() override {}

    void Shutdown();

    Scene* Copy() override;
    Scene* Copy(Container* container) override;

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    std::string GetTypeName() override { return "Scene"; }

    void AddGameObject(GameObject* obj);
    void RemoveGameObject(const std::string& obj_id){};

    void AddRootObject(const std::string& obj_id);
    void RemoveRootObject(const std::string& obj_id);

    std::vector<GameObject*> GetRootObjects();
    std::vector<GameObject*> GetAllGameObjects();

    const std::string& GetName() const { return name; }
    void SetName(const std::string& newName) { name = newName; Notify(); }

private:
    std::string name;

    bool initialized = false;

    std::vector<std::string> root_object_ids;
    std::vector<std::string> gameobject_ids;
};
