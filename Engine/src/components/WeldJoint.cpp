#include "engine/components/WeldJoint.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/ComponentImpl.hpp"
#include "engine/core/GameObjectImpl.hpp"
#include "engine/core/PhysicsSystem.hpp"
#include "engine/utils/IVisitor.hpp"
#include "yaml-cpp/yaml.h"

YAML::Node WeldJoint::Serialize(){
    YAML::Node node = Joint::Serialize();
    node["linearHertz"] = linearHertz;
    node["linearDampingRatio"] = linearDampingRatio;
    node["angularHertz"] = angularHertz;
    node["angularDampingRatio"] = angularDampingRatio;
    return node;
}

void WeldJoint::Deserialize(const YAML::Node& node){
    if (state >= State::Loaded) return;
    Joint::Deserialize(node);
    if (node["linearHertz"]) linearHertz = node["linearHertz"].as<float>();
    if (node["linearDampingRatio"]) linearDampingRatio = node["linearDampingRatio"].as<float>();
    if (node["angularHertz"]) angularHertz = node["angularHertz"].as<float>();
    if (node["angularDampingRatio"]) angularDampingRatio = node["angularDampingRatio"].as<float>();
    state = State::Loaded;
}

void WeldJoint::CreateJoint(){
    b2WeldJointDef def = b2DefaultWeldJointDef();
    FillBaseDef(def.base);
    def.linearHertz = linearHertz;
    def.linearDampingRatio = linearDampingRatio;
    def.angularHertz = angularHertz;
    def.angularDampingRatio = angularDampingRatio;
    jointId = b2CreateWeldJoint(physicsSystem->GetWorldId(), &def);
}

void WeldJoint::SetLinearHertz(float value){
    if (linearHertz == value) return;
    linearHertz = value;
    if (b2Joint_IsValid(jointId)) b2WeldJoint_SetLinearHertz(jointId, value);
    this->Notify(WeldJoint::LINEAR_HERTZ_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void WeldJoint::SetLinearDampingRatio(float value){
    if (linearDampingRatio == value) return;
    linearDampingRatio = value;
    if (b2Joint_IsValid(jointId)) b2WeldJoint_SetLinearDampingRatio(jointId, value);
    this->Notify(WeldJoint::LINEAR_DAMPING_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void WeldJoint::SetAngularHertz(float value){
    if (angularHertz == value) return;
    angularHertz = value;
    if (b2Joint_IsValid(jointId)) b2WeldJoint_SetAngularHertz(jointId, value);
    this->Notify(WeldJoint::ANGULAR_HERTZ_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void WeldJoint::SetAngularDampingRatio(float value){
    if (angularDampingRatio == value) return;
    angularDampingRatio = value;
    if (b2Joint_IsValid(jointId)) b2WeldJoint_SetAngularDampingRatio(jointId, value);
    this->Notify(WeldJoint::ANGULAR_DAMPING_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void WeldJoint::Accept(IVisitor* v) {
    v->Visit(this);
}

WeldJoint* WeldJoint::Copy(){
    WeldJoint* copy = new WeldJoint();
    CopyBaseTo(copy);
    copy->linearHertz = linearHertz;
    copy->linearDampingRatio = linearDampingRatio;
    copy->angularHertz = angularHertz;
    copy->angularDampingRatio = angularDampingRatio;
    return copy;
}

WeldJoint* WeldJoint::Copy(Container* container){
    WeldJoint* copy = this->Copy();
    copy->Attach(container);
    return copy;
}
