#include "engine/components/WheelJoint.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/ComponentImpl.hpp"
#include "engine/core/GameObjectImpl.hpp"
#include "engine/core/PhysicsSystem.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/utils/IVisitor.hpp"
#include "yaml-cpp/yaml.h"

using namespace EngineUtils::RenderUtils;
using namespace EngineUtils::MathUtils;

YAML::Node WheelJoint::Serialize(){
    YAML::Node node = Joint::Serialize();
    node["axisAngle"] = axisAngle;
    node["enableSpring"] = enableSpring;
    node["hertz"] = hertz;
    node["dampingRatio"] = dampingRatio;
    node["enableLimit"] = enableLimit;
    node["lowerTranslation"] = lowerTranslation;
    node["upperTranslation"] = upperTranslation;
    node["enableMotor"] = enableMotor;
    node["motorSpeed"] = motorSpeed;
    node["maxMotorTorque"] = maxMotorTorque;
    return node;
}

void WheelJoint::Deserialize(const YAML::Node& node){
    if (state >= State::Loaded) return;
    Joint::Deserialize(node);
    if (node["axisAngle"]) axisAngle = node["axisAngle"].as<float>();
    if (node["enableSpring"]) enableSpring = node["enableSpring"].as<bool>();
    if (node["hertz"]) hertz = node["hertz"].as<float>();
    if (node["dampingRatio"]) dampingRatio = node["dampingRatio"].as<float>();
    if (node["enableLimit"]) enableLimit = node["enableLimit"].as<bool>();
    if (node["lowerTranslation"]) lowerTranslation = node["lowerTranslation"].as<float>();
    if (node["upperTranslation"]) upperTranslation = node["upperTranslation"].as<float>();
    if (node["enableMotor"]) enableMotor = node["enableMotor"].as<bool>();
    if (node["motorSpeed"]) motorSpeed = node["motorSpeed"].as<float>();
    if (node["maxMotorTorque"]) maxMotorTorque = node["maxMotorTorque"].as<float>();
    state = State::Loaded;
}

float WheelJoint::GetLocalFrameAngleA() const {
    return axisAngle * DEG_2_RAD;
}

void WheelJoint::CreateJoint(){
    b2WheelJointDef def = b2DefaultWheelJointDef();
    FillBaseDef(def.base);
    def.enableSpring = enableSpring;
    def.hertz = hertz;
    def.dampingRatio = dampingRatio;
    def.enableLimit = enableLimit;
    def.lowerTranslation = PixelsToMeters(lowerTranslation);
    def.upperTranslation = PixelsToMeters(upperTranslation);
    def.enableMotor = enableMotor;
    def.motorSpeed = motorSpeed * DEG_2_RAD;
    def.maxMotorTorque = maxMotorTorque;
    jointId = b2CreateWheelJoint(physicsSystem->GetWorldId(), &def);
}

void WheelJoint::SetAxisAngle(float degrees){
    if (axisAngle == degrees) return;
    axisAngle = degrees;
    RefreshLocalFrames();   // the suspension axis IS frame A's rotation
    this->Notify(WheelJoint::AXIS_ANGLE_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void WheelJoint::SetEnableSpring(bool value){
    if (enableSpring == value) return;
    enableSpring = value;
    if (b2Joint_IsValid(jointId)) b2WheelJoint_EnableSpring(jointId, value);
    this->Notify(WheelJoint::ENABLE_SPRING_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void WheelJoint::SetHertz(float value){
    if (hertz == value) return;
    hertz = value;
    if (b2Joint_IsValid(jointId)) b2WheelJoint_SetSpringHertz(jointId, value);
    this->Notify(WheelJoint::HERTZ_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void WheelJoint::SetDampingRatio(float value){
    if (dampingRatio == value) return;
    dampingRatio = value;
    if (b2Joint_IsValid(jointId)) b2WheelJoint_SetSpringDampingRatio(jointId, value);
    this->Notify(WheelJoint::DAMPING_RATIO_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void WheelJoint::SetEnableLimit(bool value){
    if (enableLimit == value) return;
    enableLimit = value;
    if (b2Joint_IsValid(jointId)) b2WheelJoint_EnableLimit(jointId, value);
    this->Notify(WheelJoint::ENABLE_LIMIT_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

// Paired native setter -- pass both cached bounds on either half-setter.
void WheelJoint::SetLowerTranslation(float pixels){
    if (lowerTranslation == pixels) return;
    lowerTranslation = pixels;
    if (b2Joint_IsValid(jointId))
        b2WheelJoint_SetLimits(jointId, PixelsToMeters(lowerTranslation), PixelsToMeters(upperTranslation));
    this->Notify(WheelJoint::LIMITS_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void WheelJoint::SetUpperTranslation(float pixels){
    if (upperTranslation == pixels) return;
    upperTranslation = pixels;
    if (b2Joint_IsValid(jointId))
        b2WheelJoint_SetLimits(jointId, PixelsToMeters(lowerTranslation), PixelsToMeters(upperTranslation));
    this->Notify(WheelJoint::LIMITS_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void WheelJoint::SetEnableMotor(bool value){
    if (enableMotor == value) return;
    enableMotor = value;
    if (b2Joint_IsValid(jointId)) b2WheelJoint_EnableMotor(jointId, value);
    this->Notify(WheelJoint::ENABLE_MOTOR_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void WheelJoint::SetMotorSpeed(float degreesPerSecond){
    if (motorSpeed == degreesPerSecond) return;
    motorSpeed = degreesPerSecond;
    if (b2Joint_IsValid(jointId)) b2WheelJoint_SetMotorSpeed(jointId, motorSpeed * DEG_2_RAD);
    this->Notify(WheelJoint::MOTOR_SPEED_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void WheelJoint::SetMaxMotorTorque(float value){
    if (maxMotorTorque == value) return;
    maxMotorTorque = value;
    if (b2Joint_IsValid(jointId)) b2WheelJoint_SetMaxMotorTorque(jointId, value);
    this->Notify(WheelJoint::MAX_MOTOR_TORQUE_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void WheelJoint::Accept(IVisitor* v) {
    v->Visit(this);
}

WheelJoint* WheelJoint::Copy(){
    WheelJoint* copy = new WheelJoint();
    CopyBaseTo(copy);
    copy->axisAngle = axisAngle;
    copy->enableSpring = enableSpring;
    copy->hertz = hertz;
    copy->dampingRatio = dampingRatio;
    copy->enableLimit = enableLimit;
    copy->lowerTranslation = lowerTranslation;
    copy->upperTranslation = upperTranslation;
    copy->enableMotor = enableMotor;
    copy->motorSpeed = motorSpeed;
    copy->maxMotorTorque = maxMotorTorque;
    return copy;
}

WheelJoint* WheelJoint::Copy(Container* container){
    WheelJoint* copy = this->Copy();
    copy->Attach(container);
    return copy;
}
