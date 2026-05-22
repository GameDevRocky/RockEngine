#include "engine/components/Collider.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/ComponentImpl.hpp"
#include "engine/core/GameObjectImpl.hpp"
#include "engine/core/PhysicsSystem.hpp"
#include "yaml-cpp/yaml.h" 
#include <exception>

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
    physicsSystem = container->FindSystem<PhysicsSystem>();
    rigidBody = GetComponent<RigidBody>();
    if (!rigidBody){
        rigidBody = this->RequireComponent<RigidBody>();
        rigidBody->SetBodyType(b2_kinematicBody);

    }
    state = State::Initialized;
}

void Collider::PostInit(){
    if (state >= State::PostInitialized) return;
    Transform* transform = this->GetComponent<Transform>();
    cachedScale = transform->GetWorldScale();
    const std::string& id = this->GetID();
    transform->Subscribe([id](){
        auto* collider = Registry::FindInRuntime<Collider>(id);
        collider->OnTransformScaleUpdate();
        return true;
    }, Transform::SCALE_CHANGED_EVENT);
    state = State::PostInitialized;
}

void Collider::SetIsSensor(bool isSensor){
    if (this->isSensor == isSensor) return; 
    this->isSensor = isSensor;
    this->CreateShape();
    this->Notify(Collider::IS_SENSOR_CHANGED_EVENT);
    this->Notify(Collider::CHANGED_EVENT);
}

void Collider::SetCenter(glm::vec2 center){
    if (this->center == center) return; 
    this->center = center;
    this->CreateShape();
    this->Notify(Collider::CENTER_CHANGED_EVENT);
    this->Notify(Collider::CHANGED_EVENT);
}

void Collider::SetDensity(float density){
    if (this->density == density) return; 
    this->density = density;
    b2Shape_SetDensity(shapeId, this->density, true);
    this->Notify(Collider::DENSITY_CHANGED_EVENT);
    this->Notify(Collider::CHANGED_EVENT);
}

void Collider::SetBounciness(float bounciness){
    if (this->bounciness == bounciness) return; 
    this->bounciness = bounciness;
    b2Shape_SetRestitution(shapeId, this->bounciness);
    this->Notify(Collider::BOUNCINESS_CHANGED_EVENT);
    this->Notify(Collider::CHANGED_EVENT);
}

void Collider::SetFriction(float friction){
    if (this->friction == friction) return; 
    this->friction = friction;
    b2Shape_SetFriction(shapeId, this->friction);
    this->Notify(Collider::FRICTION_CHANGED_EVENT);
    this->Notify(Collider::CHANGED_EVENT);
}

void Collider::SetRollingResistance(float rollingResistance){
    if (this->rollingResistance == rollingResistance) return; 
    this->rollingResistance = rollingResistance;
    CreateShape();
    this->Notify(Collider::ROLLING_RESISTANCE_CHANGED_EVENT);
    this->Notify(Collider::CHANGED_EVENT);
}

void Collider::OnEnabled(){

}

void Collider::OnDisabled(){

}

void Collider::OnTransformScaleUpdate(){
    Transform* transform = GetComponent<Transform>();
    glm::vec2 worldScale = transform->GetWorldScale();
    if (cachedScale != worldScale){
        CreateShape();
        cachedScale = worldScale;
    }

}


void Collider::Accept(IVisitor* v) {
    
    v->Visit(this); 
}

void Collider::Shutdown(){
    physicsSystem->DestroyShape(shapeId);
    shapeId = b2_nullShapeId;
    Component::Shutdown();
}