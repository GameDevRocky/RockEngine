#include "engine/components/PrismaticJoint.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/ComponentImpl.hpp"
#include "engine/core/GameObjectImpl.hpp"
#include "engine/core/PhysicsSystem.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/utils/IVisitor.hpp"
#include "yaml-cpp/yaml.h"

using namespace EngineUtils::RenderUtils;
using namespace EngineUtils::MathUtils;

YAML::Node PrismaticJoint::Serialize(){
    YAML::Node node = Joint::Serialize();
    node["axisAngle"] = axisAngle;
    node["enableSpring"] = enableSpring;
    node["hertz"] = hertz;
    node["dampingRatio"] = dampingRatio;
    node["targetTranslation"] = targetTranslation;
    node["enableLimit"] = enableLimit;
    node["lowerTranslation"] = lowerTranslation;
    node["upperTranslation"] = upperTranslation;
    node["enableMotor"] = enableMotor;
    node["motorSpeed"] = motorSpeed;
    node["maxMotorForce"] = maxMotorForce;
    return node;
}

void PrismaticJoint::Deserialize(const YAML::Node& node){
    if (state >= State::Loaded) return;
    Joint::Deserialize(node);
    if (node["axisAngle"]) axisAngle = node["axisAngle"].as<float>();
    if (node["enableSpring"]) enableSpring = node["enableSpring"].as<bool>();
    if (node["hertz"]) hertz = node["hertz"].as<float>();
    if (node["dampingRatio"]) dampingRatio = node["dampingRatio"].as<float>();
    if (node["targetTranslation"]) targetTranslation = node["targetTranslation"].as<float>();
    if (node["enableLimit"]) enableLimit = node["enableLimit"].as<bool>();
    if (node["lowerTranslation"]) lowerTranslation = node["lowerTranslation"].as<float>();
    if (node["upperTranslation"]) upperTranslation = node["upperTranslation"].as<float>();
    if (node["enableMotor"]) enableMotor = node["enableMotor"].as<bool>();
    if (node["motorSpeed"]) motorSpeed = node["motorSpeed"].as<float>();
    if (node["maxMotorForce"]) maxMotorForce = node["maxMotorForce"].as<float>();
    state = State::Loaded;
}

float PrismaticJoint::GetLocalFrameAngleA() const {
    return axisAngle * DEG_2_RAD;
}

void PrismaticJoint::CreateJoint(){
    b2PrismaticJointDef def = b2DefaultPrismaticJointDef();
    FillBaseDef(def.base);
    def.enableSpring = enableSpring;
    def.hertz = hertz;
    def.dampingRatio = dampingRatio;
    def.targetTranslation = PixelsToMeters(targetTranslation);
    def.enableLimit = enableLimit;
    def.lowerTranslation = PixelsToMeters(lowerTranslation);
    def.upperTranslation = PixelsToMeters(upperTranslation);
    def.enableMotor = enableMotor;
    def.motorSpeed = PixelsToMeters(motorSpeed);
    def.maxMotorForce = maxMotorForce;
    jointId = b2CreatePrismaticJoint(physicsSystem->GetWorldId(), &def);
}

void PrismaticJoint::SetAxisAngle(float degrees){
    if (axisAngle == degrees) return;
    axisAngle = degrees;
    RefreshLocalFrames();   // the axis IS frame A's rotation, so this is a live update
    this->Notify(PrismaticJoint::AXIS_ANGLE_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void PrismaticJoint::SetEnableSpring(bool value){
    if (enableSpring == value) return;
    enableSpring = value;
    if (b2Joint_IsValid(jointId)) b2PrismaticJoint_EnableSpring(jointId, value);
    this->Notify(PrismaticJoint::ENABLE_SPRING_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void PrismaticJoint::SetHertz(float value){
    if (hertz == value) return;
    hertz = value;
    if (b2Joint_IsValid(jointId)) b2PrismaticJoint_SetSpringHertz(jointId, value);
    this->Notify(PrismaticJoint::HERTZ_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void PrismaticJoint::SetDampingRatio(float value){
    if (dampingRatio == value) return;
    dampingRatio = value;
    if (b2Joint_IsValid(jointId)) b2PrismaticJoint_SetSpringDampingRatio(jointId, value);
    this->Notify(PrismaticJoint::DAMPING_RATIO_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void PrismaticJoint::SetTargetTranslation(float pixels){
    if (targetTranslation == pixels) return;
    targetTranslation = pixels;
    if (b2Joint_IsValid(jointId))
        b2PrismaticJoint_SetTargetTranslation(jointId, PixelsToMeters(targetTranslation));
    this->Notify(PrismaticJoint::TARGET_TRANSLATION_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void PrismaticJoint::SetEnableLimit(bool value){
    if (enableLimit == value) return;
    enableLimit = value;
    if (b2Joint_IsValid(jointId)) b2PrismaticJoint_EnableLimit(jointId, value);
    this->Notify(PrismaticJoint::ENABLE_LIMIT_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

// Paired native setter -- pass both cached bounds on either half-setter.
void PrismaticJoint::SetLowerTranslation(float pixels){
    if (lowerTranslation == pixels) return;
    lowerTranslation = pixels;
    if (b2Joint_IsValid(jointId))
        b2PrismaticJoint_SetLimits(jointId, PixelsToMeters(lowerTranslation), PixelsToMeters(upperTranslation));
    this->Notify(PrismaticJoint::LIMITS_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void PrismaticJoint::SetUpperTranslation(float pixels){
    if (upperTranslation == pixels) return;
    upperTranslation = pixels;
    if (b2Joint_IsValid(jointId))
        b2PrismaticJoint_SetLimits(jointId, PixelsToMeters(lowerTranslation), PixelsToMeters(upperTranslation));
    this->Notify(PrismaticJoint::LIMITS_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void PrismaticJoint::SetEnableMotor(bool value){
    if (enableMotor == value) return;
    enableMotor = value;
    if (b2Joint_IsValid(jointId)) b2PrismaticJoint_EnableMotor(jointId, value);
    this->Notify(PrismaticJoint::ENABLE_MOTOR_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void PrismaticJoint::SetMotorSpeed(float pixelsPerSecond){
    if (motorSpeed == pixelsPerSecond) return;
    motorSpeed = pixelsPerSecond;
    if (b2Joint_IsValid(jointId)) b2PrismaticJoint_SetMotorSpeed(jointId, PixelsToMeters(motorSpeed));
    this->Notify(PrismaticJoint::MOTOR_SPEED_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void PrismaticJoint::SetMaxMotorForce(float value){
    if (maxMotorForce == value) return;
    maxMotorForce = value;
    if (b2Joint_IsValid(jointId)) b2PrismaticJoint_SetMaxMotorForce(jointId, value);
    this->Notify(PrismaticJoint::MAX_MOTOR_FORCE_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

float PrismaticJoint::GetTranslation() const {
    if (!b2Joint_IsValid(jointId)) return 0.0f;
    return MetersToPixels(b2PrismaticJoint_GetTranslation(jointId));
}

void PrismaticJoint::Accept(IVisitor* v) {
    v->Visit(this);
}

PrismaticJoint* PrismaticJoint::Copy(){
    PrismaticJoint* copy = new PrismaticJoint();
    CopyBaseTo(copy);
    copy->axisAngle = axisAngle;
    copy->enableSpring = enableSpring;
    copy->hertz = hertz;
    copy->dampingRatio = dampingRatio;
    copy->targetTranslation = targetTranslation;
    copy->enableLimit = enableLimit;
    copy->lowerTranslation = lowerTranslation;
    copy->upperTranslation = upperTranslation;
    copy->enableMotor = enableMotor;
    copy->motorSpeed = motorSpeed;
    copy->maxMotorForce = maxMotorForce;
    return copy;
}

PrismaticJoint* PrismaticJoint::Copy(Container* container){
    PrismaticJoint* copy = this->Copy();
    copy->Attach(container);
    return copy;
}
