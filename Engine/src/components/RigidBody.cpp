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
    node["lockRotation"] = lockRotation;
    return node;
}

void RigidBody::Deserialize(const YAML::Node& node){
    if (state >= State::Loaded) return;
    Component::Deserialize(node);
    std::string type = node["bodyType"].as<std::string>();
    if (type == "Dynamic") bodyType = b2BodyType::b2_dynamicBody;
    else if (type == "Kinematic") bodyType = b2BodyType::b2_kinematicBody;
    else if (type == "Static") bodyType = b2BodyType::b2_staticBody;
    useGravity = node["useGravity"].as<bool>();
    lockRotation = node["lockRotation"].as<bool>();
    bodyId = b2_nullBodyId;
    state = State::Loaded;
}


void RigidBody::Init(){
    if (state >= State::Initialized) return;
    physicsSystem = container->FindSystem<PhysicsSystem>();
    timeManager = container->FindSystem<TimeManager>();
    Transform* transform = GetTransform();
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyId = b2CreateBody(physicsSystem->GetWorldId(), &bodyDef);
    state = State::Initialized;
}


void RigidBody::PostInit(){
    if (state >= State::PostInitialized) return;

    SetBodyType(bodyType);
    SetUseGravity(useGravity);
    SetLockRotation(lockRotation);
    UpdateTransform();
    state = State::PostInitialized;
}

void RigidBody::UpdateTransform(){
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

void RigidBody::SetUseGravity(bool value){
    useGravity = value;
    if (b2Body_IsValid(bodyId)) {
        b2Body_SetGravityScale(bodyId, value ? 1.0f : 0.0f);
    }
}

bool RigidBody::GetUseGravity() const {
    return useGravity;
}

void RigidBody::SetLockRotation(bool value){
    lockRotation = value;
    // Don't use b2Body_SetMotionLocks as it creates soft constraints that interfere with collision
    // Instead, we'll zero angular velocity in FixedUpdate and LateUpdate
    if (b2Body_IsValid(bodyId) && value) {
        // Immediately zero out any existing angular velocity
        b2Body_SetAngularVelocity(bodyId, 0.0f);
    }
}

bool RigidBody::GetLockRotation() const {
    return lockRotation;
}

void RigidBody::Awake(){
    if (state >= State::Awakened) return;
    
    SetBodyType(bodyType);
    SetUseGravity(useGravity);
    SetLockRotation(lockRotation);
    state = State::Awakened;
}


void RigidBody::FixedUpdate() {
    if (bodyType == b2_dynamicBody || bodyType == b2_staticBody) return;
    if (!b2Body_IsValid(bodyId)) return;
    Transform* transform = GetTransform();

    glm::vec2 targetWorldPos = transform->GetWorldPosition();
    b2Vec2 targetPos = { targetWorldPos.x / PixelsPerUnit, targetWorldPos.y / PixelsPerUnit };
    b2Vec2 currentPos = b2Body_GetPosition(bodyId);   
    b2Vec2 velocity = {
        (targetPos.x - currentPos.x) / timeManager->FixedDeltaTime(),
        (targetPos.y - currentPos.y) / timeManager->FixedDeltaTime()
    };

    float targetRot = transform->GetWorldRotation() * DEG_2_RAD;
    float currentRot = b2Rot_GetAngle(b2Body_GetRotation(bodyId));

    b2Body_SetLinearVelocity(bodyId, velocity);
    if (lockRotation) b2Body_SetAngularVelocity(bodyId, 0);
    else b2Body_SetAngularVelocity(bodyId, (targetRot - currentRot) / timeManager->FixedDeltaTime());
}

void RigidBody::LateUpdate() {
    if (bodyType == b2_kinematicBody) return;
    
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
        b2Body_SetAngularVelocity(bodyId, vel * DEG_2_RAD);
    }
}

float RigidBody::GetAngularVelocity() const {
    if (b2Body_IsValid(bodyId)) {
        return b2Body_GetAngularVelocity(bodyId) * RAD_2_DEG;
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
        b2Body_ApplyAngularImpulse(bodyId, impulse * DEG_2_RAD, true);
    }
}

RigidBody* RigidBody::Copy(){
    RigidBody* copy = new RigidBody();
    copy->id = id;
    copy->enabled = enabled;
    copy->gameobject_id = gameobject_id;
    copy->bodyType = bodyType;
    copy->useGravity = useGravity;
    copy->bodyId = b2_nullBodyId;
    copy->lockRotation = lockRotation;
    copy->state = State::Loaded;
    return copy;
}

RigidBody* RigidBody::Copy(Container* container){
    RigidBody* copy = RigidBody::Copy();
    copy->Attach(container);
    return copy;
}