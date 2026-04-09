#pragma once 

#include "engine/components/Collider.hpp"


class CircleCollider : public Collider{

public:
    void Deserialize(const YAML::Node& node) override;
    YAML::Node Serialize() override;

    void Awake() override;
    void PostInit() override;

    std::string GetTypeName() const override {return "CircleCollider"; }

    virtual void CreateShape();

    float GetRadius(){return radius;}
    void SetRadius(float radius);
    void Accept(IVisitor* v) override;

    CircleCollider* Copy() override;
    CircleCollider* Copy(Container* container) override;

private:
    float radius = 0.0f;






};