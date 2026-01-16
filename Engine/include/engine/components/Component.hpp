#pragma once
#include <string>
#include "engine/core/GameObject.hpp"
#include "engine/serialization/Serializable.hpp"

class Component : public Serializable{
public: 

    virtual ~Component() = default;
    
    virtual YAML::Node Serialize(){
        YAML::Node node = Serializable::Serialize();
        return node;
    }
    virtual void Deserialize(const YAML::Node& node) override;
    GameObject* GetGameObject();
    virtual void OnCreated(){};
    virtual void OnEnabled(){}
    virtual void OnDisabled(){}
    void SetEnabled(bool e);
    bool GetEnabled() {return enabled;}
    virtual void Awake(){}
    virtual void Start() {}
    virtual void Update() {}
    virtual void LateUpdate(){}
    virtual void FixedUpdate(){}
    virtual void OnDestroy() {}
    virtual std::string GetTypeName() const = 0;
    
protected:
    std::string gameobject_id = "";
    bool enabled = true;

private:



};
