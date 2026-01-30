#pragma once
#include <string>
#include "engine/core/GameObject.hpp"
#include "engine/serialization/Serializable.hpp"
#include "engine/core/RuntimeObject.hpp"

class Transform;
class Component : public Serializable, public RuntimeObject{
public: 

    using Serializable::Copy;

    virtual void Init(){}
    virtual void PostInit(){}

    virtual void Awake(){}
    virtual void Update() {}
    virtual void LateUpdate(){}
    virtual void FixedUpdate(){}
    virtual void OnEnterPlayMode() override {}
    virtual void OnExitPlayMode() override {}
    virtual void OnDestroy() {}
    
    virtual YAML::Node Serialize(){
        YAML::Node node = Serializable::Serialize();
        return node;
    }
    virtual void Deserialize(const YAML::Node& node) override;

    virtual void OnEnabled(){}
    virtual void OnDisabled(){}

    Component* Copy(Container* container) override;

    GameObject* GetGameObject();

    template<typename T>
    T* GetComponent(){
        return GetGameObject()->GetComponent<T>();
    }
    template<typename T>
    T* GetComponentInParent();

    Transform* GetTransform(){
        return GetGameObject()->GetTransform();
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
