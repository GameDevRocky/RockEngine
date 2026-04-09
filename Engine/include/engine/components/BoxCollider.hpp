#pragma once
#include "engine/components/Collider.hpp"
#include "yaml-cpp/yaml.h"



class BoxCollider  : public Collider{

public:
    void Deserialize(const YAML::Node& node) override;
    YAML::Node Serialize() override;

    void Awake() override;
    void PostInit() override;

    std::string GetTypeName() const override {return "BoxCollider"; }

    virtual void CreateShape();

    glm::vec2 GetSize(){return size;}
    void SetSize(glm::vec2 size);
    void Accept(IVisitor* v) override;
    BoxCollider* Copy() override;
    BoxCollider* Copy(Container* container) override;

private:
    glm::vec2 size = {0, 0};

};