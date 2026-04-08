#pragma once
#include <string>
#include "engine/serialization/Serializable.hpp"
#include "engine/core/RuntimeObject.hpp"
#include "engine/core/GameObject.hpp"

class Transform;

class Component : public RuntimeObject{
public: 

    static inline const Event ENABLED_CHANGED_EVENT = Component::CreateEvent();

    using Serializable::Copy;

	virtual void OnCollisionEnter(GameObject* other) {}
    virtual void OnCollisionExit(GameObject* other) {}
    virtual void OnTriggerEnter(GameObject* other) {}
    virtual void OnTriggerExit(GameObject* other) {}

    virtual void LateUpdate(){}
    virtual void FixedUpdate(){}
    virtual void Destroy() {}
    
    virtual YAML::Node Serialize(){
        YAML::Node node = Serializable::Serialize();
        return node;
    }
    virtual void Deserialize(const YAML::Node& node) override;

    virtual void OnEnabled(){}
    virtual void OnDisabled(){}

    Component* Copy(Container* container) override;
    void SetGameObject(GameObject* obj);
    GameObject* GetGameObject();
    
    template<typename T>
    T* RequireComponent(){
        return GetGameObject()->RequireComponent<T>();
    }

    template<typename T>
    T* GetComponent(){
        return GetGameObject()->GetComponent<T>();
    }

    template<typename T>
    T* GetComponentInParent();

    Transform* GetTransform(){
        GameObject* obj = GetGameObject();
        Transform* transform = obj->GetTransform();
        return transform;
    }

    void SetEnabled(bool e);
    bool GetEnabled() {return enabled;}
    
    virtual std::string GetTypeName() const = 0;
    virtual ~Component() = default;
    
protected:
    std::string gameobject_id = "";
    bool enabled = true;

private:



};
