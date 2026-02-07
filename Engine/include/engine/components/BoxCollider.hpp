#pragma once
#include "engine/components/Collider.hpp"
#include "yaml-cpp/yaml.h"



class BoxCollider  : public Collider{

public:
    void Deserialize(const YAML::Node& node) override;
    YAML::Node Serialize() override;
    void Init() override ;
    void PostInit() override;
    void Awake() override;
    void Start() override;
    void Update() override;
    void OnDestroy() override;
    void SetIsSensor(bool val);
    std::string GetTypeName() const override {return "BoxCollider"; }

    void SetCenter(glm::vec2 center);
    glm::vec2 GetSize(){return size;}
    void SetSize(glm::vec2 size);

    BoxCollider* Copy() override;
    BoxCollider* Copy(Container* container) override;

private:
    glm::vec2 size = {0, 0};
    b2ShapeId shapeId;

};