#include "engine/components/RevoluteJoint.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/ComponentImpl.hpp"
#include "engine/core/GameObjectImpl.hpp"
#include "engine/core/PhysicsSystem.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/utils/IVisitor.hpp"
#include "yaml-cpp/yaml.h"

using namespace EngineUtils::MathUtils;

YAML::Node RevoluteJoint::Serialize(){
    YAML::Node node = Joint::Serialize();
    node["targetAngle"] = targetAngle;
    node["enableSpring"] = enableSpring;
    node["hertz"] = hertz;
    node["dampingRatio"] = dampingRatio;
    node["enableLimit"] = enableLimit;
    node["lowerAngle"] = lowerAngle;
    node["upperAngle"] = upperAngle;
    node["enableMotor"] = enableMotor;
    node["motorSpeed"] = motorSpeed;
    node["maxMotorTorque"] = maxMotorTorque;
    return node;
}

void RevoluteJoint::Deserialize(const YAML::Node& node){
    if (state >= State::Loaded) return;
    Joint::Deserialize(node);
    if (node["targetAngle"]) targetAngle = node["targetAngle"].as<float>();
    if (node["enableSpring"]) enableSpring = node["enableSpring"].as<bool>();
    if (node["hertz"]) hertz = node["hertz"].as<float>();
    if (node["dampingRatio"]) dampingRatio = node["dampingRatio"].as<float>();
    if (node["enableLimit"]) enableLimit = node["enableLimit"].as<bool>();
    if (node["lowerAngle"]) lowerAngle = node["lowerAngle"].as<float>();
    if (node["upperAngle"]) upperAngle = node["upperAngle"].as<float>();
    if (node["enableMotor"]) enableMotor = node["enableMotor"].as<bool>();
    if (node["motorSpeed"]) motorSpeed = node["motorSpeed"].as<float>();
    if (node["maxMotorTorque"]) maxMotorTorque = node["maxMotorTorque"].as<float>();
    state = State::Loaded;
}

void RevoluteJoint::CreateJoint(){
    b2RevoluteJointDef def = b2DefaultRevoluteJointDef();
    FillBaseDef(def.base);
    def.targetAngle = targetAngle * DEG_2_RAD;
    def.enableSpring = enableSpring;
    def.hertz = hertz;
    def.dampingRatio = dampingRatio;
    def.enableLimit = enableLimit;
    def.lowerAngle = lowerAngle * DEG_2_RAD;
    def.upperAngle = upperAngle * DEG_2_RAD;
    def.enableMotor = enableMotor;
    def.motorSpeed = motorSpeed * DEG_2_RAD;
    def.maxMotorTorque = maxMotorTorque;
    jointId = b2CreateRevoluteJoint(physicsSystem->GetWorldId(), &def);
}

void RevoluteJoint::SetTargetAngle(float degrees){
    if (targetAngle == degrees) return;
    targetAngle = degrees;
    if (b2Joint_IsValid(jointId)) b2RevoluteJoint_SetTargetAngle(jointId, targetAngle * DEG_2_RAD);
    this->Notify(RevoluteJoint::TARGET_ANGLE_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void RevoluteJoint::SetEnableSpring(bool value){
    if (enableSpring == value) return;
    enableSpring = value;
    if (b2Joint_IsValid(jointId)) b2RevoluteJoint_EnableSpring(jointId, value);
    this->Notify(RevoluteJoint::ENABLE_SPRING_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void RevoluteJoint::SetHertz(float value){
    if (hertz == value) return;
    hertz = value;
    if (b2Joint_IsValid(jointId)) b2RevoluteJoint_SetSpringHertz(jointId, value);
    this->Notify(RevoluteJoint::HERTZ_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void RevoluteJoint::SetDampingRatio(float value){
    if (dampingRatio == value) return;
    dampingRatio = value;
    if (b2Joint_IsValid(jointId)) b2RevoluteJoint_SetSpringDampingRatio(jointId, value);
    this->Notify(RevoluteJoint::DAMPING_RATIO_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void RevoluteJoint::SetEnableLimit(bool value){
    if (enableLimit == value) return;
    enableLimit = value;
    if (b2Joint_IsValid(jointId)) b2RevoluteJoint_EnableLimit(jointId, value);
    this->Notify(RevoluteJoint::ENABLE_LIMIT_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

// b2RevoluteJoint_SetLimits takes both bounds at once, so each half-setter passes
// the other cached bound along with it.
void RevoluteJoint::SetLowerAngle(float degrees){
    if (lowerAngle == degrees) return;
    lowerAngle = degrees;
    if (b2Joint_IsValid(jointId))
        b2RevoluteJoint_SetLimits(jointId, lowerAngle * DEG_2_RAD, upperAngle * DEG_2_RAD);
    this->Notify(RevoluteJoint::LIMITS_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void RevoluteJoint::SetUpperAngle(float degrees){
    if (upperAngle == degrees) return;
    upperAngle = degrees;
    if (b2Joint_IsValid(jointId))
        b2RevoluteJoint_SetLimits(jointId, lowerAngle * DEG_2_RAD, upperAngle * DEG_2_RAD);
    this->Notify(RevoluteJoint::LIMITS_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void RevoluteJoint::SetEnableMotor(bool value){
    if (enableMotor == value) return;
    enableMotor = value;
    if (b2Joint_IsValid(jointId)) b2RevoluteJoint_EnableMotor(jointId, value);
    this->Notify(RevoluteJoint::ENABLE_MOTOR_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void RevoluteJoint::SetMotorSpeed(float degreesPerSecond){
    if (motorSpeed == degreesPerSecond) return;
    motorSpeed = degreesPerSecond;
    if (b2Joint_IsValid(jointId)) b2RevoluteJoint_SetMotorSpeed(jointId, motorSpeed * DEG_2_RAD);
    this->Notify(RevoluteJoint::MOTOR_SPEED_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void RevoluteJoint::SetMaxMotorTorque(float value){
    if (maxMotorTorque == value) return;
    maxMotorTorque = value;
    if (b2Joint_IsValid(jointId)) b2RevoluteJoint_SetMaxMotorTorque(jointId, value);
    this->Notify(RevoluteJoint::MAX_MOTOR_TORQUE_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

float RevoluteJoint::GetAngle() const {
    if (!b2Joint_IsValid(jointId)) return 0.0f;
    return b2RevoluteJoint_GetAngle(jointId) * RAD_2_DEG;
}

void RevoluteJoint::Accept(IVisitor* v) {
    v->Visit(this);
}

RevoluteJoint* RevoluteJoint::Copy(){
    RevoluteJoint* copy = new RevoluteJoint();
    CopyBaseTo(copy);
    copy->targetAngle = targetAngle;
    copy->enableSpring = enableSpring;
    copy->hertz = hertz;
    copy->dampingRatio = dampingRatio;
    copy->enableLimit = enableLimit;
    copy->lowerAngle = lowerAngle;
    copy->upperAngle = upperAngle;
    copy->enableMotor = enableMotor;
    copy->motorSpeed = motorSpeed;
    copy->maxMotorTorque = maxMotorTorque;
    return copy;
}

RevoluteJoint* RevoluteJoint::Copy(Container* container){
    RevoluteJoint* copy = this->Copy();
    copy->Attach(container);
    return copy;
}
