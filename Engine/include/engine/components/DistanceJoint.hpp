#pragma once
#include "engine/components/Joint.hpp"

// Holds two anchor points a fixed distance apart -- ropes, tethers, springs.
// Lengths are authored in pixels and converted at the Box2D boundary.
class DistanceJoint : public Joint {

public:
    static inline const Event LENGTH_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event ENABLE_SPRING_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event HERTZ_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event DAMPING_RATIO_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event SPRING_FORCE_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event ENABLE_LIMIT_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event LENGTH_RANGE_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event ENABLE_MOTOR_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event MOTOR_SPEED_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event MAX_MOTOR_FORCE_CHANGED_EVENT = Joint::CreateEvent();

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;

    void SetLength(float pixels);
    float GetLength() const { return length; }
    void SetEnableSpring(bool value);
    bool GetEnableSpring() const { return enableSpring; }
    void SetHertz(float value);
    float GetHertz() const { return hertz; }
    void SetDampingRatio(float value);
    float GetDampingRatio() const { return dampingRatio; }
    void SetLowerSpringForce(float value);
    float GetLowerSpringForce() const { return lowerSpringForce; }
    void SetUpperSpringForce(float value);
    float GetUpperSpringForce() const { return upperSpringForce; }
    void SetEnableLimit(bool value);
    bool GetEnableLimit() const { return enableLimit; }
    void SetMinLength(float pixels);
    float GetMinLength() const { return minLength; }
    void SetMaxLength(float pixels);
    float GetMaxLength() const { return maxLength; }
    void SetEnableMotor(bool value);
    bool GetEnableMotor() const { return enableMotor; }
    void SetMotorSpeed(float pixelsPerSecond);
    float GetMotorSpeed() const { return motorSpeed; }
    void SetMaxMotorForce(float value);
    float GetMaxMotorForce() const { return maxMotorForce; }

    // Live readout, in pixels. Falls back to the authored length before the joint exists.
    float GetCurrentLength() const;

    void Accept(IVisitor* v) override;
    std::string GetTypeName() const override { return "DistanceJoint"; }

    DistanceJoint* Copy() override;
    DistanceJoint* Copy(Container* container) override;

    DistanceJoint() = default;

protected:
    void CreateJoint() override;

private:
    float length = 32.0f;            // pixels
    bool  enableSpring = false;
    float hertz = 4.0f;
    float dampingRatio = 0.5f;
    float lowerSpringForce = -1000000000.0f;
    float upperSpringForce = 1000000000.0f;
    bool  enableLimit = false;
    float minLength = 0.0f;          // pixels
    float maxLength = 320.0f;        // pixels
    bool  enableMotor = false;
    float motorSpeed = 0.0f;         // pixels/second
    float maxMotorForce = 0.0f;
};
