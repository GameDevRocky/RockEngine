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
    // Detaches a component and queues it for deferred destruction (flushed next
    // frame). Fires REMOVE_COMPONENT_EVENT so observers (e.g. the inspector) refresh.
    void RemoveComponent(Component* comp);


    // Returns the first attached component castable to T (insertion order), or
    // nullptr. With multiple components of the same type this yields the oldest.
    template<typename T>
    T* GetComponent() {
        Registry* registry = container->FindSystem<Registry>();
        if (!registry) return nullptr;
        for (const auto& comp_id : component_ids) {
            if (T* comp = registry->Find<T>(comp_id)) return comp;
        }
        return nullptr;
    }

    // Returns every attached component castable to T, in insertion order.
    template<typename T>
    std::vector<T*> GetComponents() {
        std::vector<T*> result;
        Registry* registry = container->FindSystem<Registry>();
        if (!registry) return result;
        for (const auto& comp_id : component_ids) {
            if (T* comp = registry->Find<T>(comp_id)) result.push_back(comp);
        }
        return result;
    }

    template<typename T>
    T* GetComponentInParent();

    template<typename T> 
    T* RequireComponent();

    std::vector<Component*> GetAllComponents();
    
    Transform* GetTransform();
    std::string GetTypeName() override {return "GameObject";}
    void Accept(IVisitor* v) override;
    void SetName(const std::string& name);
    std::string GetName() {return name;}
    void SetActive(bool val);
    bool GetActive(){return active;}

    void SetTag(const std::string& tag);
    const std::string& GetTag() const { return tag; }

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

    // True if any attached component reports this type name. Resolves each id via
    // the registry (component_ids is no longer keyed by type).
    bool HasComponentByName(const std::string& type_name) const;

    private:
    bool active = true;
    std::string tag = "Untagged";
    std::vector<std::string> component_ids;   // component ids in insertion order
    std::string transform_id;
    std::string scene_id;    
};
