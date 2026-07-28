#pragma once
#include "engine/components/Joint.hpp"

// Pins two bodies at a point and lets them rotate about it -- hinges, doors,
// ragdoll limbs, wheels without suspension. Angles are authored in degrees.
class RevoluteJoint : public Joint {

public:
    static inline const Event TARGET_ANGLE_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event ENABLE_SPRING_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event HERTZ_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event DAMPING_RATIO_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event ENABLE_LIMIT_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event LIMITS_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event ENABLE_MOTOR_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event MOTOR_SPEED_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event MAX_MOTOR_TORQUE_CHANGED_EVENT = Joint::CreateEvent();

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;

    void SetTargetAngle(float degrees);
    float GetTargetAngle() const { return targetAngle; }
    void SetEnableSpring(bool value);
    bool GetEnableSpring() const { return enableSpring; }
    void SetHertz(float value);
    float GetHertz() const { return hertz; }
    void SetDampingRatio(float value);
    float GetDampingRatio() const { return dampingRatio; }
    void SetEnableLimit(bool value);
    bool GetEnableLimit() const { return enableLimit; }
    void SetLowerAngle(float degrees);
    float GetLowerAngle() const { return lowerAngle; }
    void SetUpperAngle(float degrees);
    float GetUpperAngle() const { return upperAngle; }
    void SetEnableMotor(bool value);
    bool GetEnableMotor() const { return enableMotor; }
    void SetMotorSpeed(float degreesPerSecond);
    float GetMotorSpeed() const { return motorSpeed; }
    void SetMaxMotorTorque(float value);
    float GetMaxMotorTorque() const { return maxMotorTorque; }

    // Live readout in degrees; 0 before the joint exists.
    float GetAngle() const;

    void Accept(IVisitor* v) override;
    std::string GetTypeName() const override { return "RevoluteJoint"; }

    RevoluteJoint* Copy() override;
    RevoluteJoint* Copy(Container* container) override;

    RevoluteJoint() = default;

protected:
    void CreateJoint() override;

private:
    float targetAngle = 0.0f;     // degrees
    bool  enableSpring = false;
    float hertz = 4.0f;
    float dampingRatio = 0.5f;
    bool  enableLimit = false;
    float lowerAngle = -90.0f;    // degrees
    float upperAngle = 90.0f;     // degrees
    bool  enableMotor = false;
    float motorSpeed = 0.0f;      // degrees/second
    float maxMotorTorque = 0.0f;
};
