#pragma once
#include "engine/components/Joint.hpp"

// Rigidly fuses two bodies. Hertz of 0 means maximum stiffness; raising it turns
// the weld springy, which is how you fake soft-body wobble. No motor, no limits.
class WeldJoint : public Joint {

public:
    static inline const Event LINEAR_HERTZ_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event LINEAR_DAMPING_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event ANGULAR_HERTZ_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event ANGULAR_DAMPING_CHANGED_EVENT = Joint::CreateEvent();

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;

    void SetLinearHertz(float value);
    float GetLinearHertz() const { return linearHertz; }
    void SetLinearDampingRatio(float value);
    float GetLinearDampingRatio() const { return linearDampingRatio; }
    void SetAngularHertz(float value);
    float GetAngularHertz() const { return angularHertz; }
    void SetAngularDampingRatio(float value);
    float GetAngularDampingRatio() const { return angularDampingRatio; }

    void Accept(IVisitor* v) override;
    std::string GetTypeName() const override { return "WeldJoint"; }

    WeldJoint* Copy() override;
    WeldJoint* Copy(Container* container) override;

    WeldJoint() = default;

protected:
    void CreateJoint() override;

private:
    float linearHertz = 0.0f;           // 0 == rigid
    float linearDampingRatio = 1.0f;
    float angularHertz = 0.0f;          // 0 == rigid
    float angularDampingRatio = 1.0f;
};
