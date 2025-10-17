#pragma once
#include <string>
#include "engine/core/GameObject.hpp"
#include "engine/serialization/Serializable.hpp"

class Component : public Serializable{
public:
    GameObject* gameobject = nullptr;
    bool enabled = true;

    virtual ~Component() = default;
    
    virtual YAML::Node Serialize(){
        YAML::Node node = Serializable::Serialize();
        return node;
    }
    virtual void Deserialize(YAML::Node node){
        Serializable::Deserialize(node);
    }
    virtual void Awake(){}
    virtual void Start() {}
    virtual void Update() {}
    virtual void LateUpdate(){}
    virtual void FixedUpdate(){}
    virtual void OnDestroy() {}

    virtual std::string GetTypeName() const = 0;
};
