#pragma once
#include <vector>
#include <unordered_map>
#include <type_traits>
#include <typeinfo>
#include <string>
#include "engine/serialization/Serializable.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/core/Scene.hpp"
#include <iostream>
#include "Engine.hpp"
#include "engine/core/RuntimeObject.hpp"
#include <algorithm> 
#include <atomic>

class Component;
class Transform;
class Registry;

class GameObject : public RuntimeObject {
    
    public:
    void Deserialize(const YAML::Node& node) override;

    void Init() override;
    void PostInit() override;
    void Awake() override;
    void Start() override;
    void Update() override;
    
    void FixedUpdate();
    void LateUpdate();

    GameObject* Copy() override;
    GameObject* Copy(Container* container) override;
    
    std::string name;
    GameObject();
    ~GameObject() =default;
    
    void AddComponent(Component* comp);
    
    
    template<typename T>
    T* GetComponent() {
        std::string type = std::string(EngineUtils::TypeName<T>());
        auto it = component_ids.find(type);
        if (it == component_ids.end())
        return nullptr;
        
        Registry* registry = container->FindSystem<Registry>();
        const std::string& comp_id = it->second;
        T* comp = registry->Find<T>(comp_id);
        return comp;
    }
    
    template<typename T>
    T* GetComponentInParent(); // Implemented in GameObjectImpl.hpp

    template<typename T> 
    T* RequireComponent();
    
    Transform* GetTransform();
    std::string GetTypeName() override {return "GameObject";}
    std::string GetName() {return name;}
    void SetActive(bool val){active = val;}
    bool GetActive(){return active;}

    void SetIsRootObject(bool val){isRootObject = val;}
    bool GetIsRootObject(){return isRootObject;}

    void SetScene(const std::string& id);
    Scene* GetScene();
    
    template<typename T>
    void recurseTopDown(T callback) {
        callback(this);
        for (auto& child : GetTransform()->GetChildren()) {
            child->GetGameObject()->recurseTopDown(callback);
        }
    }

    template<typename T>
    void recurseBottomUp(T callback) {
        for (auto& child : GetTransform()->GetChildren()) {
            child->GetGameObject()->recurseBottomUp(callback);
        }
        callback(this); // Execute on Parent LAST
    }
    
    uint32_t GetPickingID() const { return pickingId; }

    private:
    static std::atomic<uint32_t> next_id;
    uint32_t pickingId;

    bool active = true;
    bool isRootObject = false;
    std::map<std::string, std::string> component_ids;
    std::string transform_id;
    std::vector<std::string> temp_ids;
    std::string scene_id;
    
    Registry* registry = nullptr;

};
