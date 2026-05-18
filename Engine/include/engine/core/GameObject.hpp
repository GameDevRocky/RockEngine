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
    static inline const Event ACTIVE_CHANGED_EVENT = GameObject::CreateEvent();
    static inline const Event ADD_COMPONENT_EVENT = GameObject::CreateEvent();
    static inline const Event REMOVE_COMPONENT_EVENT = GameObject::CreateEvent();
    static inline const Event SCENE_CHANGED_EVENT = GameObject::CreateEvent();
    static inline const Event NAME_CHANGED_EVENT = GameObject::CreateEvent();
    static inline const Event TAG_CHANGED_EVENT = GameObject::CreateEvent();
    static inline const Event LAYER_CHANGED_EVENT = GameObject::CreateEvent();

    void Deserialize(const YAML::Node& node) override;

    void Init() override;
    void PostInit() override;
    void Awake() override;
    void Start() override;
    void Update() override;
    void OnCollisionEnter(GameObject* other);
    void OnCollisionExit(GameObject* other);
    void OnTriggerEnter(GameObject* other);
    void OnTriggerExit(GameObject* other);
    void Shutdown() override;
    
    void FixedUpdate();
    void LateUpdate();

    GameObject* Copy() override;
    GameObject* Copy(Container* container) override;
    
    std::string name;
    GameObject() = default;
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
    T* GetComponentInParent();

    template<typename T> 
    T* RequireComponent();

    std::vector<Component*> GetAllComponents();
    
    Transform* GetTransform();
    std::string GetTypeName() override {return "GameObject";}
    void SetName(const std::string& name);
    std::string GetName() {return name;}
    void SetActive(bool val);
    bool GetActive(){return active;}

    void SetScene(Scene* scene);
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
        callback(this);
    }

    private:
    bool active = true;
    std::map<std::string, std::string> component_ids;
    std::string transform_id;
    std::string scene_id;
    
};
