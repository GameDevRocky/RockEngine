#pragma once
#include <string>
#include "engine/core/GameObject.hpp"
#include "engine/serialization/Serializable.hpp"
#include "engine/core/RuntimeObject.hpp"

class Transform;
class Component : public RuntimeObject{
public: 

    using Serializable::Copy;

    
    virtual void LateUpdate(){}
    virtual void FixedUpdate(){}
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
