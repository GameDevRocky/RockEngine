#include "engine/components/Collider.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/core/GameObjectImpl.hpp"
#include "yaml-cpp/yaml.h"

void Collider::Deserialize(const YAML::Node& node){
    Component::Deserialize(node);
    isSensor = node["isSensor"].as<bool>();
    density = node["density"].as<float>();
    friction = node["friction"].as<float>();
    bounciness = node["bounciness"].as<float>();
    rollingResistance = node["rollingResistance"].as<float>();
    float x = node["center"][0].as<float>();
    float y = node["center"][1].as<float> ();
    center = {x, y};
    state = State::Loaded;
}

void Collider::Init(){
    if (state >= State::Initialized) return;
    rigidBody = this->RequireComponent<RigidBody>();
    state = State::Initialized;
}

void Collider::SetIsSensor(bool isSensor){
    this->isSensor = isSensor;
    this->CreateShape();
}

void Collider::SetCenter(glm::vec2 center){
    this->center = center;
    this->CreateShape();
}

void Collider::SetDensity(float density){
    this->density = density;
    b2Shape_SetDensity(shapeId, this->density, true);
}

void Collider::SetBounciness(float bounciness){
    this->bounciness = bounciness;
    b2Shape_SetRestitution(shapeId, this->bounciness);
}

void Collider::SetFriction(float friction){
    this->friction = friction;
    b2Shape_SetFriction(shapeId, this->friction);
}

void Collider::SetRollingResistance(float rollingResistance){
    this->rollingResistance = rollingResistance;
    CreateShape();
}



