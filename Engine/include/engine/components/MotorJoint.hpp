#pragma once
#include "engine/components/Joint.hpp"
#include "glm/glm.hpp"

// Drives the relative velocity between two bodies. With zero velocity it behaves
// like top-down friction; with a velocity it acts as a conveyor or a soft drive.
// Has no anchors or limits by nature -- the inherited anchors stay at zero.
class MotorJoint : public Joint {

public:
    static inline const Event LINEAR_VELOCITY_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event MAX_VELOCITY_FORCE_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event ANGULAR_VELOCITY_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event MAX_VELOCITY_TORQUE_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event LINEAR_HERTZ_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event LINEAR_DAMPING_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event MAX_SPRING_FORCE_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event ANGULAR_HERTZ_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event ANGULAR_DAMPING_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event MAX_SPRING_TORQUE_CHANGED_EVENT = Joint::CreateEvent();

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;

    void SetLinearVelocity(glm::vec2 pixelsPerSecond);
    glm::vec2 GetLinearVelocity() const { return linearVelocity; }
    void SetMaxVelocityForce(float value);
    float GetMaxVelocityForce() const { return maxVelocityForce; }
    void SetAngularVelocity(float degreesPerSecond);
    float GetAngularVelocity() const { return angularVelocity; }
    void SetMaxVelocityTorque(float value);
    float GetMaxVelocityTorque() const { return maxVelocityTorque; }
    void SetLinearHertz(float value);
    float GetLinearHertz() const { return linearHertz; }
    void SetLinearDampingRatio(float value);
    float GetLinearDampingRatio() const { return linearDampingRatio; }
    void SetMaxSpringForce(float value);
    float GetMaxSpringForce() const { return maxSpringForce; }
    void SetAngularHertz(float value);
    float GetAngularHertz() const { return angularHertz; }
    void SetAngularDampingRatio(float value);
    float GetAngularDampingRatio() const { return angularDampingRatio; }
    void SetMaxSpringTorque(float value);
    float GetMaxSpringTorque() const { return maxSpringTorque; }

    void Accept(IVisitor* v) override;
    std::string GetTypeName() const override { return "MotorJoint"; }

    MotorJoint* Copy() override;
    MotorJoint* Copy(Container* container) override;

    MotorJoint() = default;

protected:
    void CreateJoint() override;

private:
    glm::vec2 linearVelocity = {0, 0};   // pixels/second
    float maxVelocityForce = 0.0f;
    float angularVelocity = 0.0f;        // degrees/second
    float maxVelocityTorque = 0.0f;
    float linearHertz = 0.0f;
    float linearDampingRatio = 0.0f;
    float maxSpringForce = 0.0f;
    float angularHertz = 0.0f;
    float angularDampingRatio = 0.0f;
    float maxSpringTorque = 0.0f;
};
