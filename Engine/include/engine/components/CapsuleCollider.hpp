#pragma once
#include "engine/components/Collider.hpp"
#include "yaml-cpp/yaml.h"




class CapsuleCollider : public Collider{

public:
    void Deserialize(const YAML::Node& node);
    virtual void Awake() override;
    virtual void PostInit() override;
    std::string GetTypeName() const override {return "CapsuleCollider";}
    virtual void CreateShape();

    CapsuleCollider* Copy() override;
    CapsuleCollider* Copy(Container* container) override;
    




};