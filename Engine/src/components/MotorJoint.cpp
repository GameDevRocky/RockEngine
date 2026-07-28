#include "engine/components/MotorJoint.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/ComponentImpl.hpp"
#include "engine/core/GameObjectImpl.hpp"
#include "engine/core/PhysicsSystem.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/utils/IVisitor.hpp"
#include "yaml-cpp/yaml.h"

using namespace EngineUtils::RenderUtils;
using namespace EngineUtils::MathUtils;

YAML::Node MotorJoint::Serialize(){
    YAML::Node node = Joint::Serialize();
    node["linearVelocity"][0] = linearVelocity.x;
    node["linearVelocity"][1] = linearVelocity.y;
    node["linearVelocity"].SetStyle(YAML::EmitterStyle::Flow);
    node["maxVelocityForce"] = maxVelocityForce;
    node["angularVelocity"] = angularVelocity;
    node["maxVelocityTorque"] = maxVelocityTorque;
    node["linearHertz"] = linearHertz;
    node["linearDampingRatio"] = linearDampingRatio;
    node["maxSpringForce"] = maxSpringForce;
    node["angularHertz"] = angularHertz;
    node["angularDampingRatio"] = angularDampingRatio;
    node["maxSpringTorque"] = maxSpringTorque;
    return node;
}

void MotorJoint::Deserialize(const YAML::Node& node){
    if (state >= State::Loaded) return;
    Joint::Deserialize(node);
    if (node["linearVelocity"])
        linearVelocity = {node["linearVelocity"][0].as<float>(), node["linearVelocity"][1].as<float>()};
    if (node["maxVelocityForce"]) maxVelocityForce = node["maxVelocityForce"].as<float>();
    if (node["angularVelocity"]) angularVelocity = node["angularVelocity"].as<float>();
    if (node["maxVelocityTorque"]) maxVelocityTorque = node["maxVelocityTorque"].as<float>();
    if (node["linearHertz"]) linearHertz = node["linearHertz"].as<float>();
    if (node["linearDampingRatio"]) linearDampingRatio = node["linearDampingRatio"].as<float>();
    if (node["maxSpringForce"]) maxSpringForce = node["maxSpringForce"].as<float>();
    if (node["angularHertz"]) angularHertz = node["angularHertz"].as<float>();
    if (node["angularDampingRatio"]) angularDampingRatio = node["angularDampingRatio"].as<float>();
    if (node["maxSpringTorque"]) maxSpringTorque = node["maxSpringTorque"].as<float>();
    state = State::Loaded;
}

void MotorJoint::CreateJoint(){
    b2MotorJointDef def = b2DefaultMotorJointDef();
    FillBaseDef(def.base);
    def.linearVelocity = {PixelsToMeters(linearVelocity.x), PixelsToMeters(linearVelocity.y)};
    def.maxVelocityForce = maxVelocityForce;
    def.angularVelocity = angularVelocity * DEG_2_RAD;
    def.maxVelocityTorque = maxVelocityTorque;
    def.linearHertz = linearHertz;
    def.linearDampingRatio = linearDampingRatio;
    def.maxSpringForce = maxSpringForce;
    def.angularHertz = angularHertz;
    def.angularDampingRatio = angularDampingRatio;
    def.maxSpringTorque = maxSpringTorque;
    jointId = b2CreateMotorJoint(physicsSystem->GetWorldId(), &def);
}

void MotorJoint::SetLinearVelocity(glm::vec2 pixelsPerSecond){
    if (linearVelocity == pixelsPerSecond) return;
    linearVelocity = pixelsPerSecond;
    if (b2Joint_IsValid(jointId))
        b2MotorJoint_SetLinearVelocity(jointId,
            {PixelsToMeters(linearVelocity.x), PixelsToMeters(linearVelocity.y)});
    this->Notify(MotorJoint::LINEAR_VELOCITY_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void MotorJoint::SetMaxVelocityForce(float value){
    if (maxVelocityForce == value) return;
    maxVelocityForce = value;
    if (b2Joint_IsValid(jointId)) b2MotorJoint_SetMaxVelocityForce(jointId, value);
    this->Notify(MotorJoint::MAX_VELOCITY_FORCE_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void MotorJoint::SetAngularVelocity(float degreesPerSecond){
    if (angularVelocity == degreesPerSecond) return;
    angularVelocity = degreesPerSecond;
    if (b2Joint_IsValid(jointId)) b2MotorJoint_SetAngularVelocity(jointId, angularVelocity * DEG_2_RAD);
    this->Notify(MotorJoint::ANGULAR_VELOCITY_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void MotorJoint::SetMaxVelocityTorque(float value){
    if (maxVelocityTorque == value) return;
    maxVelocityTorque = value;
    if (b2Joint_IsValid(jointId)) b2MotorJoint_SetMaxVelocityTorque(jointId, value);
    this->Notify(MotorJoint::MAX_VELOCITY_TORQUE_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void MotorJoint::SetLinearHertz(float value){
    if (linearHertz == value) return;
    linearHertz = value;
    if (b2Joint_IsValid(jointId)) b2MotorJoint_SetLinearHertz(jointId, value);
    this->Notify(MotorJoint::LINEAR_HERTZ_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void MotorJoint::SetLinearDampingRatio(float value){
    if (linearDampingRatio == value) return;
    linearDampingRatio = value;
    if (b2Joint_IsValid(jointId)) b2MotorJoint_SetLinearDampingRatio(jointId, value);
    this->Notify(MotorJoint::LINEAR_DAMPING_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void MotorJoint::SetMaxSpringForce(float value){
    if (maxSpringForce == value) return;
    maxSpringForce = value;
    if (b2Joint_IsValid(jointId)) b2MotorJoint_SetMaxSpringForce(jointId, value);
    this->Notify(MotorJoint::MAX_SPRING_FORCE_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void MotorJoint::SetAngularHertz(float value){
    if (angularHertz == value) return;
    angularHertz = value;
    if (b2Joint_IsValid(jointId)) b2MotorJoint_SetAngularHertz(jointId, value);
    this->Notify(MotorJoint::ANGULAR_HERTZ_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void MotorJoint::SetAngularDampingRatio(float value){
    if (angularDampingRatio == value) return;
    angularDampingRatio = value;
    if (b2Joint_IsValid(jointId)) b2MotorJoint_SetAngularDampingRatio(jointId, value);
    this->Notify(MotorJoint::ANGULAR_DAMPING_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void MotorJoint::SetMaxSpringTorque(float value){
    if (maxSpringTorque == value) return;
    maxSpringTorque = value;
    if (b2Joint_IsValid(jointId)) b2MotorJoint_SetMaxSpringTorque(jointId, value);
    this->Notify(MotorJoint::MAX_SPRING_TORQUE_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void MotorJoint::Accept(IVisitor* v) {
    v->Visit(this);
}

MotorJoint* MotorJoint::Copy(){
    MotorJoint* copy = new MotorJoint();
    CopyBaseTo(copy);
    copy->linearVelocity = linearVelocity;
    copy->maxVelocityForce = maxVelocityForce;
    copy->angularVelocity = angularVelocity;
    copy->maxVelocityTorque = maxVelocityTorque;
    copy->linearHertz = linearHertz;
    copy->linearDampingRatio = linearDampingRatio;
    copy->maxSpringForce = maxSpringForce;
    copy->angularHertz = angularHertz;
    copy->angularDampingRatio = angularDampingRatio;
    copy->maxSpringTorque = maxSpringTorque;
    return copy;
}

MotorJoint* MotorJoint::Copy(Container* container){
    MotorJoint* copy = this->Copy();
    copy->Attach(container);
    return copy;
}
