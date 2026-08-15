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
#include <cstdint>


class Component;
class Transform;
class Registry;

class GameObject : public RuntimeObject {
    
    public:
    static inline const Event ACTIVE_CHANGED_EVENT = GameObject::CreateEvent();
    static inline const Event ADD_COMPONENT_EVENT = GameObject::CreateEvent();
    static inline const Event REMOVE_COMPONENT_EVENT = GameObject::CreateEvent();
    static inline const Event COMPONENT_ORDER_CHANGED_EVENT = GameObject::CreateEvent();
    static inline const Event SCENE_CHANGED_EVENT = GameObject::CreateEvent();
    static inline const Event NAME_CHANGED_EVENT = GameObject::CreateEvent();
    static inline const Event TAG_CHANGED_EVENT = GameObject::CreateEvent();
    static inline const Event LAYER_CHANGED_EVENT = GameObject::CreateEvent();

    YAML::Node Serialize() override;
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

    // Moves an attached component to a final index in the authoritative ordered
    // component list. The order is serialized and also drives component
    // lifecycle iteration.
    bool MoveComponent(const std::string& componentId, std::size_t targetIndex);
    int GetComponentIndex(const std::string& componentId) const;
    const std::vector<std::string>& GetComponentOrder() const { return component_ids; }


    // Returns the first attached component castable to T (insertion order), or
    // nullptr. With multiple components of the same type this yields the oldest.
    // Iterates the resolved-pointer cache (rebuilt only when the registry
    // generation moves), so the per-frame cost is a few dynamic_casts rather
    // than a string-hash + Find per component id.
    template<typename T>
    T* GetComponent() {
        for (Component* comp : ResolvedComponents()) {
            if (T* c = dynamic_cast<T*>(comp)) return c;
        }
        return nullptr;
    }

    // Returns every attached component castable to T, in insertion order.
    template<typename T>
    std::vector<T*> GetComponents() {
        std::vector<T*> result;
        for (Component* comp : ResolvedComponents()) {
            if (T* c = dynamic_cast<T*>(comp)) result.push_back(c);
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
    void recurseTopDown(const T& callback) {
        callback(this);
        // ChildObjects() (defined in the .cpp, where Transform is complete)
        // keeps this header free of Transform's members — otherwise the
        // non-dependent Transform access would need the full type at parse time.
        for (GameObject* child : ChildObjects())
            child->recurseTopDown(callback);
    }

    template<typename T>
    void recurseBottomUp(const T& callback) {
        for (GameObject* child : ChildObjects())
            child->recurseBottomUp(callback);
        callback(this);
    }

    // True if any attached component reports this type name. Resolves each id via
    // the registry (component_ids is no longer keyed by type).
    bool HasComponentByName(const std::string& type_name) const;

    private:
    // Resolved component pointers for the current registry generation, in
    // component_ids order. The per-frame update loops and every GetComponent<T>
    // walk this instead of re-resolving ids through the registry each call.
    const std::vector<Component*>& ResolvedComponents();
    // Resolved child GameObjects (this transform's children -> their owners),
    // generation-cached. Defined in the .cpp so the recurse templates above
    // never touch Transform's incomplete type in this header.
    const std::vector<GameObject*>& ChildObjects();

    bool active = true;
    std::string tag = "Untagged";
    std::vector<std::string> component_ids;   // component ids in insertion order
    std::string transform_id;
    std::string scene_id;

    // Runtime caches — never serialize, never copy. Rebuilt lazily when the
    // registry generation moves (or component_ids is mutated directly).
    std::vector<Component*> resolvedComponents;
    std::uint64_t resolvedComponentsGen = 0;
    Transform* cachedTransform = nullptr;
    std::uint64_t cachedTransformGen = 0;
    std::vector<GameObject*> cachedChildObjects;
    std::uint64_t cachedChildObjectsGen = 0;
};
