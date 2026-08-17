#pragma once
#include "engine/components/Component.hpp"
#include "box2d/box2d.h"
#include "yaml-cpp/yaml.h"

class TimeManager;
class PhysicsSystem;
class Container;

class RigidBody : public Component{

public:
    static inline const Event USE_GRAVITY_CHANGED_EVENT = RigidBody::CreateEvent();
    static inline const Event LOCK_ROTATION_CHANGED_EVENT = RigidBody::CreateEvent();
    static inline const Event BODY_TYPE_CHANGED_EVENT = RigidBody::CreateEvent();

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;

    void Init() override;
    void PostInit() override;
    void Awake() override;
    void FixedUpdate() override;
    void LateUpdate() override;
    void UpdateTransform();
    void OnEnabled() override;
    void OnDisabled() override;
    void Shutdown() override;

    void SetBodyType(b2BodyType type);
    b2BodyType GetBodyType(){return bodyType;}
    void SetUseGravity(bool value);
    bool GetUseGravity() const;
    void SetLockRotation(bool value);
    bool GetLockRotation() const;
    b2BodyId GetBodyId(){return bodyId;}
    
    void SetLinearVelocity(const glm::vec2& vel);
    glm::vec2 GetLinearVelocity() const;
    void SetAngularVelocity(float vel);
    float GetAngularVelocity() const;
    void ApplyForce(const glm::vec2& force, const glm::vec2& point);
    void ApplyForceToCenter(const glm::vec2& force);
    void ApplyImpulse(const glm::vec2& impulse, const glm::vec2& point);
    void ApplyLinearImpulse(const glm::vec2& impulse);
    void ApplyAngularImpulse(float impulse);
    void ApplyTorque(float torque);
    
    std::string GetTypeName() const override {return "RigidBody";}
    void Accept(IVisitor* v) override;

    RigidBody* Copy() override;
    RigidBody* Copy(Container* container) override;

    RigidBody() = default;
    ~RigidBody() = default;

private:
    // Which half of the pose an external write touched. Position and rotation are
    // independent axes, so a change to one must not discard the momentum of the
    // other — see OnTransformChanged.
    enum class TransformChannel { Position, Rotation };
    void OnTransformChanged(TransformChannel channel);
    
    PhysicsSystem* physicsSystem = nullptr;
    TimeManager* timeManager = nullptr;
    b2BodyId bodyId = b2_nullBodyId;
    b2BodyType bodyType = b2BodyType::b2_dynamicBody;
    bool useGravity = true;
    bool lockRotation = false;
    bool writingToTransform = false;

    // Last pose written back to the Transform in LateUpdate — runtime cache,
    // never serialize, never copy. Lets a body that hasn't moved skip the
    // SetWorld* calls (and the MarkDirty walk + change notifies they trigger).
    b2Vec2 lastSyncedPos = { 0.0f, 0.0f };
    float lastSyncedAngle = 0.0f;
    bool hasSyncedPose = false;

};