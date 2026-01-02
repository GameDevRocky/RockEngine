#pragma once
#include <string>
#include "ISerializable.hpp"

class IGameObject;

class IComponent : public ISerializable{
public: 
    virtual ~IComponent() = 0;
    virtual YAML::Node Serialize(){
        YAML::Node node = ISerializable::Serialize();
        return node;
    }
    virtual void Deserialize(const YAML::Node& node) override = 0;
    virtual IGameObject* GetGameObject() = 0;
    virtual void OnEnabled() = 0;
    virtual void OnDisabled() = 0;
    virtual void SetEnabled(bool e) = 0;
    virtual void Awake() = 0;
    virtual void Start() = 0;
    virtual void Update() = 0;
    virtual void LateUpdate() = 0;
    virtual void FixedUpdate() = 0;
    virtual void OnDestroy() = 0;
    virtual std::string GetTypeName() = 0;
    virtual IComponent* Create() = 0; 
};
