#pragma once
#include "engine/components/Collider.hpp"
#include "yaml-cpp/yaml.h"




class CapsuleCollider : public Collider {

public:
    void Deserialize(const YAML::Node& node);
    virtual void Awake() override;
    virtual void PostInit() override;
    std::string GetTypeName() const override {return "CapsuleCollider";}
    virtual void CreateShape();

    void SetHeight(float height);
    float GetHeight(){return height;}

    void SetRadius(float radius);
    float GetRadius(){return radius;}
    void Accept(IVisitor* v) override;
    CapsuleCollider* Copy() override;
    CapsuleCollider* Copy(Container* container) override;


    
private:
    float radius = 1.0f;
    float height = 1.0f;

};