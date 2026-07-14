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

    // Our Box2D shape lives on the RigidBody's body, so when the RigidBody is
    // destroyed Box2D tears the body (and our shape) down with it — this Collider
    // must go too. Tie our lifetime to the RigidBody's shutdown rather than relying
    // on teardown order. One-shot: returns false to auto-unsubscribe.
    const std::string colliderId = GetID();
    rigidBody->Subscribe([colliderId](){
        Collider* self = Registry::FindInRuntime<Collider>(colliderId);
        if (!self) return false;
        if (GameObject* go = self->GetGameObject())
            go->RemoveComponent(self);
        return false;
    }, RuntimeObject::SHUTDOWN_EVENT);

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
    // While disabled the live shape is forced to 0 density (see OnDisabled) so it
    // doesn't contribute mass; don't overwrite that. `density` is still updated above
    // so OnEnabled restores the correct value.
    if (GetEnabled() && b2Shape_IsValid(shapeId)) b2Shape_SetDensity(shapeId, this->density, true);
    this->Notify(Collider::DENSITY_CHANGED_EVENT);
    this->Notify(Collider::CHANGED_EVENT);
}

void Collider::SetBounciness(float bounciness){
    if (this->bounciness == bounciness) return; 
    this->bounciness = bounciness;
    if (b2Shape_IsValid(shapeId)) b2Shape_SetRestitution(shapeId, this->bounciness);
    this->Notify(Collider::BOUNCINESS_CHANGED_EVENT);
    this->Notify(Collider::CHANGED_EVENT);
}

void Collider::SetFriction(float friction){
    if (this->friction == friction) return; 
    this->friction = friction;
    if (b2Shape_IsValid(shapeId)) b2Shape_SetFriction(shapeId, this->friction);
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
    if (b2Shape_IsValid(shapeId)) {
        b2Shape_SetFilter(shapeId, filter);
        b2Shape_SetDensity(shapeId, density, true);
    }
    Component::OnEnabled();
}

void Collider::OnDisabled(){
    if (b2Shape_IsValid(shapeId)) {
        b2Shape_SetFilter(shapeId, b2Filter{});
        b2Shape_SetDensity(shapeId, 0.0f, true);
    }
    Component::OnDisabled();
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