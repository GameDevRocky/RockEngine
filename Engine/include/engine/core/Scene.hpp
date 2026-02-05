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

class Scene : public Serializable, public RuntimeObject{
public:
    enum class SceneState {
        Unloaded,      // Fresh scene, not deserialized
        Deserialized,  // Loaded from file, objects created
        Initialized,   // Init() + PostInit() called
        Active         // Awake() called, ready for gameplay
    };

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
    
    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void PostDeserialize() override ;
    std::string GetTypeName() override { return "Scene"; }
    
    void AddGameObject(GameObject* obj);
    void RemoveGameObject(GameObject* obj){};
    
    std::vector<GameObject*> GetRootObjects();
    std::vector<GameObject*> GetAllGameObjects();
    
    const std::string& GetName() const { return name; }
    void SetName(const std::string& newName) { name = newName; Notify(); }
    
    Scene* Copy() override;
    Scene* Copy(Container* container) override;
    SceneState GetState() const { return state; }

private:
    
    std::vector<std::string> rootobject_ids;
    std::string name;
    SceneState state = SceneState::Unloaded;
    Registry* registry;



    std::vector<GameObject*> temp_objs;
    std::vector<Component*> temp_comps;


};
