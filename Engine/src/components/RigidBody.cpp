#include "engine/components/RigidBody.hpp"
#include "engine/core/PhysicsSystem.hpp"
#include "engine/components/Transform.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/core/TimeManager.hpp"
#include <iostream>

using namespace EngineUtils::RenderUtils;
using namespace EngineUtils::MathUtils;

YAML::Node RigidBody::Serialize(){
    YAML::Node node = Component::Serialize();
    return node;
}

void RigidBody::Deserialize(const YAML::Node& node){
    if (state >= State::Loaded) return;
    Component::Deserialize(node);
    std::string type = node["bodyType"].as<std::string>();
    if (type == "Dynamic") SetBodyType(b2BodyType::b2_dynamicBody);
    else if (type == "Kinematic") SetBodyType(b2BodyType::b2_kinematicBody);
    else if (type == "Static") SetBodyType(b2BodyType::b2_staticBody);
    SetUseGravity(node["useGravity"].as<bool>());
    SetMass(node["mass"].as<float>());
    bodyId = b2_nullBodyId;
    state = State::Loaded;
}
void RigidBody::Init(){
    if (state >= State::Initialized) return;
    Transform* transform = GetTransform();
    // transform->Subscribe( [this](){
    //     this->OnUpdateTransform();
    // });
    physicsSystem = container->FindSystem<PhysicsSystem>();
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyId = b2CreateBody(physicsSystem->GetWorldId(), &bodyDef);
    SetMass(mass);
    SetBodyType(bodyType);
    SetUseGravity(useGravity);
    OnUpdateTransform();
    state = State::Initialized;
}

void RigidBody::OnUpdateTransform(){
    Transform* transform = this->GetTransform();
    glm::vec2 worldPos = transform->GetWorldPosition();
    float worldRot = transform->GetWorldRotation();
    b2Vec2 pos = {worldPos.x / PixelsPerUnit, worldPos.y / PixelsPerUnit};
    b2Rot rot = b2MakeRot(worldRot * DEG_2_RAD);
    b2Body_SetTransform(bodyId, pos, rot);
}

void RigidBody::SetBodyType(b2BodyType type){
    bodyType = type;
    if (b2Body_IsValid(bodyId)) {
        b2Body_SetType(bodyId, type);
        b2Body_SetAwake(bodyId, true);
    }
}

void RigidBody::SetMass(float newMass){
    mass = newMass;
    if (b2Body_IsValid(bodyId)) {
        b2MassData currentMassData = b2Body_GetMassData(bodyId);
        
        if (currentMassData.mass > 0.0f) {
            float massScale = newMass / currentMassData.mass;
            b2MassData newMassData;
            newMassData.mass = newMass;
            newMassData.center = currentMassData.center;
            newMassData.rotationalInertia = currentMassData.rotationalInertia * massScale;
            
            b2Body_SetMassData(bodyId, newMassData);
        }
    }
}

float RigidBody::GetMass() const {
    if (b2Body_IsValid(bodyId)) {
        return b2Body_GetMassData(bodyId).mass;
    }
    return mass;
}

void RigidBody::SetUseGravity(bool value){
    useGravity = value;
    if (b2Body_IsValid(bodyId)) {
        b2Body_SetGravityScale(bodyId, value ? 1.0f : 0.0f);
    }
}

bool RigidBody::GetUseGravity() const {
    return useGravity;
}

void RigidBody::Awake(){
    if (state >= State::Awakened) return;
    
    SetBodyType(bodyType);
    SetMass(mass);
    SetUseGravity(useGravity);
    state = State::Awakened;
}


void RigidBody::FixedUpdate() {
    if (bodyType != b2_kinematicBody) return;
    TimeManager* tm = container->FindSystem<TimeManager>();

    Transform* transform = GetTransform();
    if (!transform || !b2Body_IsValid(bodyId)) return;

    glm::vec2 targetWorldPos = transform->GetWorldPosition();
    b2Vec2 targetPos = { targetWorldPos.x / PixelsPerUnit, targetWorldPos.y / PixelsPerUnit };
    b2Vec2 currentPos = b2Body_GetPosition(bodyId);   
    b2Vec2 velocity = {
        (targetPos.x - currentPos.x) / tm->FixedDeltaTime(),
        (targetPos.y - currentPos.y) / tm->FixedDeltaTime()
    };

    b2Body_SetLinearVelocity(bodyId, velocity);
    float targetRot = transform->GetWorldRotation() * DEG_2_RAD;
    float currentRot = b2Rot_GetAngle(b2Body_GetRotation(bodyId));
    b2Body_SetAngularVelocity(bodyId, (targetRot - currentRot) / tm->FixedDeltaTime());
}

void RigidBody::LateUpdate() {
    if (bodyType != b2_dynamicBody && bodyType != b2_staticBody) return;
    
    Transform* transform = GetTransform();
    if (!transform || !b2Body_IsValid(bodyId)) return;

    b2Vec2 physicsPos = b2Body_GetPosition(bodyId);
    b2Rot physicsRot = b2Body_GetRotation(bodyId);

    float renderX = physicsPos.x * PixelsPerUnit;
    float renderY = physicsPos.y * PixelsPerUnit;
    float renderAngle = b2Rot_GetAngle(physicsRot) * RAD_2_DEG;

    transform->SetWorldPosition({renderX, renderY});
    transform->SetWorldRotation(renderAngle);
}

void RigidBody::SetLinearVelocity(const glm::vec2& vel) {
    if (b2Body_IsValid(bodyId)) {
        b2Body_SetLinearVelocity(bodyId, {vel.x / PixelsPerUnit, vel.y / PixelsPerUnit});
    }
}

glm::vec2 RigidBody::GetLinearVelocity() const {
    if (b2Body_IsValid(bodyId)) {
        b2Vec2 vel = b2Body_GetLinearVelocity(bodyId);
        return {vel.x * PixelsPerUnit, vel.y * PixelsPerUnit};
    }
    return {0.0f, 0.0f};
}

void RigidBody::SetAngularVelocity(float vel) {
    if (b2Body_IsValid(bodyId)) {
        b2Body_SetAngularVelocity(bodyId, -vel * DEG_2_RAD);
    }
}

float RigidBody::GetAngularVelocity() const {
    if (b2Body_IsValid(bodyId)) {
        return -b2Body_GetAngularVelocity(bodyId) * RAD_2_DEG;
    }
    return 0.0f;
}

void RigidBody::ApplyForce(const glm::vec2& force, const glm::vec2& point) {
    if (b2Body_IsValid(bodyId)) {
        b2Vec2 f = {force.x / PixelsPerUnit, force.y / PixelsPerUnit};
        b2Vec2 p = {point.x / PixelsPerUnit, point.y / PixelsPerUnit};
        b2Body_ApplyForce(bodyId, f, p, true);
    }
}

void RigidBody::ApplyForceToCenter(const glm::vec2& force) {
    if (b2Body_IsValid(bodyId)) {
        b2Vec2 f = {force.x / PixelsPerUnit, force.y / PixelsPerUnit};
        b2Body_ApplyForceToCenter(bodyId, f, true);
    }
}

void RigidBody::ApplyImpulse(const glm::vec2& impulse, const glm::vec2& point) {
    if (b2Body_IsValid(bodyId)) {
        b2Vec2 i = {impulse.x / PixelsPerUnit, impulse.y / PixelsPerUnit};
        b2Vec2 p = {point.x / PixelsPerUnit, point.y / PixelsPerUnit};
        b2Body_ApplyLinearImpulse(bodyId, i, p, true);
    }
}

void RigidBody::ApplyLinearImpulse(const glm::vec2& impulse) {
    if (b2Body_IsValid(bodyId)) {
        b2Vec2 i = {impulse.x / PixelsPerUnit, impulse.y / PixelsPerUnit};
        b2Body_ApplyLinearImpulseToCenter(bodyId, i, true);
    }
}

void RigidBody::ApplyAngularImpulse(float impulse) {
    if (b2Body_IsValid(bodyId)) {
        b2Body_ApplyAngularImpulse(bodyId, -impulse * DEG_2_RAD, true);
    }
}

RigidBody* RigidBody::Copy(){
    RigidBody* copy = new RigidBody();
    copy->id = id;
    copy->enabled = enabled;
    copy->gameobject_id = gameobject_id;
    copy->bodyId = b2_nullBodyId;
    copy->bodyType = bodyType;
    copy->useGravity = useGravity;
    copy->mass = mass;
    copy->state = State::Loaded;
    return copy;
}

RigidBody* RigidBody::Copy(Container* container){
    RigidBody* copy = RigidBody::Copy();
    copy->Attach(container);
    return copy;
}