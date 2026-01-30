#pragma once
#include "engine/components/Component.hpp"
#include "box2d/box2d.h"
#include "yaml-cpp/yaml.h"

class PhysicsSystem;
class Container;

class RigidBody : public Component{

public:
    static constexpr float PTM = 32.0f;

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void PostDeserialize() override{};

    void Init() override;
    void PostInit() override;
    void Awake() override;
    void Update() override;
    void FixedUpdate() override{};
    void LateUpdate() override{};
    void OnDestroy() override{};
    void OnEnterPlayMode() override{};
    void OnExitPlayMode() override{}; 
    
    std::string GetTypeName() const override {return "RigidBody";}

    RigidBody* Copy() override;
    RigidBody* Copy(Container* container) override;

    RigidBody() = default;
    ~RigidBody() = default;

private:
    b2BodyId bodyId = b2_nullBodyId;
    PhysicsSystem* physicsSystem = nullptr;


};