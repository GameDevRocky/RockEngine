#pragma once
#include "engine/components/Joint.hpp"

// Lets body B slide along a single axis relative to body A without rotating --
// elevators, sliding doors, moving platforms.
//
// The slide axis is the local +X of body A's joint frame, so axisAngle rotates
// that frame: 0 slides horizontally, 90 slides vertically.
class PrismaticJoint : public Joint {

public:
    static inline const Event AXIS_ANGLE_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event ENABLE_SPRING_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event HERTZ_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event DAMPING_RATIO_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event TARGET_TRANSLATION_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event ENABLE_LIMIT_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event LIMITS_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event ENABLE_MOTOR_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event MOTOR_SPEED_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event MAX_MOTOR_FORCE_CHANGED_EVENT = Joint::CreateEvent();

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
    void SetTargetTranslation(float pixels);
    float GetTargetTranslation() const { return targetTranslation; }
    void SetEnableLimit(bool value);
    bool GetEnableLimit() const { return enableLimit; }
    void SetLowerTranslation(float pixels);
    float GetLowerTranslation() const { return lowerTranslation; }
    void SetUpperTranslation(float pixels);
    float GetUpperTranslation() const { return upperTranslation; }
    void SetEnableMotor(bool value);
    bool GetEnableMotor() const { return enableMotor; }
    void SetMotorSpeed(float pixelsPerSecond);
    float GetMotorSpeed() const { return motorSpeed; }
    void SetMaxMotorForce(float value);
    float GetMaxMotorForce() const { return maxMotorForce; }

    // Live readout in pixels; 0 before the joint exists.
    float GetTranslation() const;

    void Accept(IVisitor* v) override;
    std::string GetTypeName() const override { return "PrismaticJoint"; }

    PrismaticJoint* Copy() override;
    PrismaticJoint* Copy(Container* container) override;

    PrismaticJoint() = default;

protected:
    void CreateJoint() override;
    // The slide axis lives in body A's frame rotation; body B's frame stays unrotated.
    float GetLocalFrameAngleA() const override;

private:
    float axisAngle = 0.0f;            // degrees
    bool  enableSpring = false;
    float hertz = 4.0f;
    float dampingRatio = 0.5f;
    float targetTranslation = 0.0f;    // pixels
    bool  enableLimit = false;
    float lowerTranslation = -320.0f;  // pixels
    float upperTranslation = 320.0f;   // pixels
    bool  enableMotor = false;
    float motorSpeed = 0.0f;           // pixels/second
    float maxMotorForce = 0.0f;
};
