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

    static inline const Event HIERARCHY_CHANGED_EVENT = Scene::CreateEvent();
    static inline const Event NAME_CHANGED_EVENT = Scene::CreateEvent();
    


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
    void Sync(GameObject* obj);
    void SyncRootObjects(const std::string& child_id, const std::string& parent_id);
    void SyncAllObjects(const std::string& id);
    
    std::vector<GameObject*> GetRootObjects();
    std::vector<GameObject*> GetAllGameObjects();
    
    const std::string& GetName() const { return name; }
    void SetName(const std::string& newName);
    
    Scene* Copy() override;
    Scene* Copy(Container* container) override;



    Scene() = default;
    ~Scene() = default;



private:
    
    std::string name;
    std::vector<std::string> rootobject_ids;
    std::vector<std::string> gameobject_ids;


};
