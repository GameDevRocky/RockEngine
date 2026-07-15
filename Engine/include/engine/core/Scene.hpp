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
    static inline const Event GAMEOBJECT_ADDED_EVENT = Scene::CreateEvent();
    // Fired after a sibling reorder (payload: the moved object's id).
    static inline const Event ORDER_CHANGED_EVENT = Scene::CreateEvent();
    


    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;

    void Init() override;
    void PostInit() override;
    void Awake() override;
    void Start() override;
    void Enable();
    void Shutdown() override;
    void Update() override;
    void FixedUpdate();
    void LateUpdate();
    
    std::string GetTypeName() override { return "Scene"; }
    
    void AddGameObject(GameObject* obj);
    void Sync(GameObject* obj);
    void SyncRootObjects(const std::string& child_id, const std::string& parent_id);
    void SyncAllObjects(const std::string& id);

    // Move an object to targetIndex among the children of parentId (empty = scene
    // roots), reparenting first if it currently sits under a different parent.
    // targetIndex counts positions in the sibling list that still contains the object.
    void ReorderObject(const std::string& id, const std::string& parentId, int targetIndex);
    
    std::vector<GameObject*> GetRootObjects();
    std::vector<GameObject*> GetAllGameObjects();
    
    const std::string& GetName() const { return name; }
    void SetName(const std::string& newName);

    const std::string& GetPath() const { return path; }
    void SetPath(const std::string& newPath) { path = newPath; }
    
    Scene* Copy() override;
    Scene* Copy(Container* container) override;



    Scene() = default;
    ~Scene() = default;



private:
    
    std::string name;
    std::string path;
    std::vector<std::string> rootobject_ids;
    std::vector<std::string> gameobject_ids;


};
