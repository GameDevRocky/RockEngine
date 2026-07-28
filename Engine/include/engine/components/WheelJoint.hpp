#pragma once
#include "engine/components/Joint.hpp"

// A wheel that spins freely while sliding along a sprung axis -- vehicle
// suspension. Combines a prismatic axis (the suspension travel) with free
// rotation and an optional drive motor.
//
// As with PrismaticJoint, the travel axis is local +X of body A's joint frame;
// axisAngle of 90 gives the usual vertical suspension.
class WheelJoint : public Joint {

public:
    static inline const Event AXIS_ANGLE_CHANGED_EVENT = Joint::CreateEvent();
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

    void SetAxisAngle(float degrees);
    float GetAxisAngle() const { return axisAngle; }
    void SetEnableSpring(bool value);
    bool GetEnableSpring() const { return enableSpring; }
    void SetHertz(float value);
    float GetHertz() const { return hertz; }
    void SetDampingRatio(float value);
    float GetDampingRatio() const { return dampingRatio; }
    void SetEnableLimit(bool value);
    bool GetEnableLimit() const { return enableLimit; }
    void SetLowerTranslation(float pixels);
    float GetLowerTranslation() const { return lowerTranslation; }
    void SetUpperTranslation(float pixels);
    float GetUpperTranslation() const { return upperTranslation; }
    void SetEnableMotor(bool value);
    bool GetEnableMotor() const { return enableMotor; }
    void SetMotorSpeed(float degreesPerSecond);
    float GetMotorSpeed() const { return motorSpeed; }
    void SetMaxMotorTorque(float value);
    float GetMaxMotorTorque() const { return maxMotorTorque; }

    void Accept(IVisitor* v) override;
    std::string GetTypeName() const override { return "WheelJoint"; }

    WheelJoint* Copy() override;
    WheelJoint* Copy(Container* container) override;

    WheelJoint() = default;

protected:
    void CreateJoint() override;
    float GetLocalFrameAngleA() const override;

private:
    float axisAngle = 90.0f;           // degrees -- vertical suspension by default
    bool  enableSpring = true;
    float hertz = 4.0f;
    float dampingRatio = 0.7f;
    bool  enableLimit = false;
    float lowerTranslation = -32.0f;   // pixels
    float upperTranslation = 32.0f;    // pixels
    bool  enableMotor = false;
    float motorSpeed = 0.0f;           // degrees/second
    float maxMotorTorque = 0.0f;
};
