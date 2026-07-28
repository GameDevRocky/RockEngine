#include "engine/components/DistanceJoint.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/ComponentImpl.hpp"
#include "engine/core/GameObjectImpl.hpp"
#include "engine/core/PhysicsSystem.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/utils/IVisitor.hpp"
#include "yaml-cpp/yaml.h"

using namespace EngineUtils::RenderUtils;

YAML::Node DistanceJoint::Serialize(){
    YAML::Node node = Joint::Serialize();
    node["length"] = length;
    node["enableSpring"] = enableSpring;
    node["hertz"] = hertz;
    node["dampingRatio"] = dampingRatio;
    node["lowerSpringForce"] = lowerSpringForce;
    node["upperSpringForce"] = upperSpringForce;
    node["enableLimit"] = enableLimit;
    node["minLength"] = minLength;
    node["maxLength"] = maxLength;
    node["enableMotor"] = enableMotor;
    node["motorSpeed"] = motorSpeed;
    node["maxMotorForce"] = maxMotorForce;
    return node;
}

void DistanceJoint::Deserialize(const YAML::Node& node){
    if (state >= State::Loaded) return;
    Joint::Deserialize(node);
    if (node["length"]) length = node["length"].as<float>();
    if (node["enableSpring"]) enableSpring = node["enableSpring"].as<bool>();
    if (node["hertz"]) hertz = node["hertz"].as<float>();
    if (node["dampingRatio"]) dampingRatio = node["dampingRatio"].as<float>();
    if (node["lowerSpringForce"]) lowerSpringForce = node["lowerSpringForce"].as<float>();
    if (node["upperSpringForce"]) upperSpringForce = node["upperSpringForce"].as<float>();
    if (node["enableLimit"]) enableLimit = node["enableLimit"].as<bool>();
    if (node["minLength"]) minLength = node["minLength"].as<float>();
    if (node["maxLength"]) maxLength = node["maxLength"].as<float>();
    if (node["enableMotor"]) enableMotor = node["enableMotor"].as<bool>();
    if (node["motorSpeed"]) motorSpeed = node["motorSpeed"].as<float>();
    if (node["maxMotorForce"]) maxMotorForce = node["maxMotorForce"].as<float>();
    state = State::Loaded;
}

void DistanceJoint::CreateJoint(){
    b2DistanceJointDef def = b2DefaultDistanceJointDef();
    FillBaseDef(def.base);
    def.length = PixelsToMeters(length);
    def.enableSpring = enableSpring;
    def.hertz = hertz;
    def.dampingRatio = dampingRatio;
    def.lowerSpringForce = lowerSpringForce;
    def.upperSpringForce = upperSpringForce;
    def.enableLimit = enableLimit;
    def.minLength = PixelsToMeters(minLength);
    def.maxLength = PixelsToMeters(maxLength);
    def.enableMotor = enableMotor;
    def.motorSpeed = PixelsToMeters(motorSpeed);
    def.maxMotorForce = maxMotorForce;
    jointId = b2CreateDistanceJoint(physicsSystem->GetWorldId(), &def);
}

void DistanceJoint::SetLength(float pixels){
    if (length == pixels) return;
    length = pixels;
    if (b2Joint_IsValid(jointId)) b2DistanceJoint_SetLength(jointId, PixelsToMeters(length));
    this->Notify(DistanceJoint::LENGTH_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void DistanceJoint::SetEnableSpring(bool value){
    if (enableSpring == value) return;
    enableSpring = value;
    if (b2Joint_IsValid(jointId)) b2DistanceJoint_EnableSpring(jointId, value);
    this->Notify(DistanceJoint::ENABLE_SPRING_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void DistanceJoint::SetHertz(float value){
    if (hertz == value) return;
    hertz = value;
    if (b2Joint_IsValid(jointId)) b2DistanceJoint_SetSpringHertz(jointId, value);
    this->Notify(DistanceJoint::HERTZ_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void DistanceJoint::SetDampingRatio(float value){
    if (dampingRatio == value) return;
    dampingRatio = value;
    if (b2Joint_IsValid(jointId)) b2DistanceJoint_SetSpringDampingRatio(jointId, value);
    this->Notify(DistanceJoint::DAMPING_RATIO_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

// Box2D takes the force range as one paired call, so each half-setter has to pass
// the other (already-cached) bound alongside it.
void DistanceJoint::SetLowerSpringForce(float value){
    if (lowerSpringForce == value) return;
    lowerSpringForce = value;
    if (b2Joint_IsValid(jointId))
        b2DistanceJoint_SetSpringForceRange(jointId, lowerSpringForce, upperSpringForce);
    this->Notify(DistanceJoint::SPRING_FORCE_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void DistanceJoint::SetUpperSpringForce(float value){
    if (upperSpringForce == value) return;
    upperSpringForce = value;
    if (b2Joint_IsValid(jointId))
        b2DistanceJoint_SetSpringForceRange(jointId, lowerSpringForce, upperSpringForce);
    this->Notify(DistanceJoint::SPRING_FORCE_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void DistanceJoint::SetEnableLimit(bool value){
    if (enableLimit == value) return;
    enableLimit = value;
    if (b2Joint_IsValid(jointId)) b2DistanceJoint_EnableLimit(jointId, value);
    this->Notify(DistanceJoint::ENABLE_LIMIT_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void DistanceJoint::SetMinLength(float pixels){
    if (minLength == pixels) return;
    minLength = pixels;
    if (b2Joint_IsValid(jointId))
        b2DistanceJoint_SetLengthRange(jointId, PixelsToMeters(minLength), PixelsToMeters(maxLength));
    this->Notify(DistanceJoint::LENGTH_RANGE_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void DistanceJoint::SetMaxLength(float pixels){
    if (maxLength == pixels) return;
    maxLength = pixels;
    if (b2Joint_IsValid(jointId))
        b2DistanceJoint_SetLengthRange(jointId, PixelsToMeters(minLength), PixelsToMeters(maxLength));
    this->Notify(DistanceJoint::LENGTH_RANGE_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void DistanceJoint::SetEnableMotor(bool value){
    if (enableMotor == value) return;
    enableMotor = value;
    if (b2Joint_IsValid(jointId)) b2DistanceJoint_EnableMotor(jointId, value);
    this->Notify(DistanceJoint::ENABLE_MOTOR_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void DistanceJoint::SetMotorSpeed(float pixelsPerSecond){
    if (motorSpeed == pixelsPerSecond) return;
    motorSpeed = pixelsPerSecond;
    if (b2Joint_IsValid(jointId)) b2DistanceJoint_SetMotorSpeed(jointId, PixelsToMeters(motorSpeed));
    this->Notify(DistanceJoint::MOTOR_SPEED_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void DistanceJoint::SetMaxMotorForce(float value){
    if (maxMotorForce == value) return;
    maxMotorForce = value;
    if (b2Joint_IsValid(jointId)) b2DistanceJoint_SetMaxMotorForce(jointId, value);
    this->Notify(DistanceJoint::MAX_MOTOR_FORCE_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

float DistanceJoint::GetCurrentLength() const {
    if (!b2Joint_IsValid(jointId)) return length;
    return MetersToPixels(b2DistanceJoint_GetCurrentLength(jointId));
}

void DistanceJoint::Accept(IVisitor* v) {
    v->Visit(this);
}

DistanceJoint* DistanceJoint::Copy(){
    DistanceJoint* copy = new DistanceJoint();
    CopyBaseTo(copy);
    copy->length = length;
    copy->enableSpring = enableSpring;
    copy->hertz = hertz;
    copy->dampingRatio = dampingRatio;
    copy->lowerSpringForce = lowerSpringForce;
    copy->upperSpringForce = upperSpringForce;
    copy->enableLimit = enableLimit;
    copy->minLength = minLength;
    copy->maxLength = maxLength;
    copy->enableMotor = enableMotor;
    copy->motorSpeed = motorSpeed;
    copy->maxMotorForce = maxMotorForce;
    return copy;
}

DistanceJoint* DistanceJoint::Copy(Container* container){
    DistanceJoint* copy = this->Copy();
    copy->Attach(container);
    return copy;
}
