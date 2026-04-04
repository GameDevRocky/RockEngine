#include "engine/components/Collider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/ComponentImpl.hpp"
#include "engine/core/GameObjectImpl.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/utils/EngineUtils.hpp"

#include <cmath>

using namespace EngineUtils::RenderUtils;

void CapsuleCollider::Deserialize(const YAML::Node& node){
    Collider::Deserialize(node);
    radius = node["radius"].as<float>();
    height = node["height"].as<float>();
    state = State::Loaded;
}

void CapsuleCollider::PostInit(){
    if (state >= State::PostInitialized) return;
    Collider::PostInit();
    if (radius <= 0.0f ||  height <= 0.0f) {
        SpriteRenderer* renderer = GetComponent<SpriteRenderer>();
        if (!renderer) {
            renderer = GetComponentInParent<SpriteRenderer>();
        }
        if (renderer && renderer->GetSprite()) {
            glm::vec2 pixelSize = renderer->GetSprite()->GetPixelSize();
            glm::vec2 worldSize = PixelsToWorld(pixelSize);
            radius = worldSize.x;
            height = worldSize.y - radius / 2.0f;
        } else {
            radius = 1.0f;
            height = 1.0f;
        }
    }
    state = State::PostInitialized;
    
}

void CapsuleCollider::Awake(){
    
    if (state >= State::Awakened) return;
    
    if (!rigidBody) {
        std::cerr << "CapsuleCollider::Awake - RigidBody is null on GameObject: " << GetGameObject()->GetName() << std::endl;
        state = State::Awakened;
        return;
    }
    
    b2BodyId bodyId = rigidBody->GetBodyId();

    if (!b2Body_IsValid(bodyId)) {
        std::cerr << "CapsuleCollider::Awake - RigidBody has invalid bodyId on GameObject: " << GetGameObject()->GetName() << std::endl;
        state = State::Awakened;
        return;
    }
    CreateShape();
    state = State::Awakened;
}

void CapsuleCollider::CreateShape(){
    if (!rigidBody) {
        std::cerr << "CapsuleCollider::CreateShape - RigidBody is null" << std::endl;
        return;
    }
    
    Transform* transform = GetTransform();
    if (!transform) {
        std::cerr << "CapsuleCollider::CreateShape - Transform is null" << std::endl;
        return;
    }
    
    b2ShapeDef definition = b2DefaultShapeDef();
    b2BodyId bodyId = rigidBody->GetBodyId();

    definition.material.friction = friction;
    definition.material.restitution = bounciness;
    definition.material.rollingResistance = rollingResistance;
    definition.density = density;
    definition.filter = filter;
    definition.isSensor = isSensor;

    glm::vec2 worldScale = transform->GetWorldScale();
    glm::vec2 scaledSize = glm::vec2(radius, height) * worldScale;
    b2Vec2 physicsCenter = {center.x * worldScale.x / PixelsPerUnit, center.y * worldScale.y / PixelsPerUnit};

    Transform* bodyTransform = rigidBody->GetTransform();
    if (bodyTransform && bodyTransform != transform) {
        glm::vec2 delta = transform->GetWorldPosition() - bodyTransform->GetWorldPosition();
        float bodyRotationRad = glm::radians(bodyTransform->GetWorldRotation());
        float c = std::cos(-bodyRotationRad);
        float s = std::sin(-bodyRotationRad);
        glm::vec2 localDelta = {
            delta.x * c - delta.y * s,
            delta.x * s + delta.y * c
        };

        physicsCenter.x += localDelta.x / PixelsPerUnit;
        physicsCenter.y += localDelta.y / PixelsPerUnit;
    }
    
    float physicsHalfWidth = scaledSize.x / (2.0f * PixelsPerUnit);
    float physicsHalfHeight = scaledSize.y / (2.0f * PixelsPerUnit);

    b2Capsule capsule = b2Capsule();
    capsule.center1 = {physicsCenter.x, physicsCenter.y + physicsHalfHeight};
    capsule.center2 = {physicsCenter.x, physicsCenter.y - physicsHalfHeight};
    capsule.radius = physicsHalfWidth;

    if (b2Shape_IsValid(shapeId)) b2DestroyShape(shapeId, true);    
    shapeId = b2CreateCapsuleShape(bodyId, &definition, &capsule);
    b2Body_ApplyMassFromShapes(bodyId);
    
}

void CapsuleCollider::SetHeight(float height){
    this->height = height;
    this->CreateShape();
    this->Notify(CapsuleCollider::CHANGED_EVENT);
}

void CapsuleCollider::SetRadius(float radius){
    this->radius = radius;
    this->CreateShape();
    this->Notify(CapsuleCollider::CHANGED_EVENT);
}

CapsuleCollider* CapsuleCollider::Copy(){
    CapsuleCollider* copy = new CapsuleCollider();
    copy->id = id;
    copy->gameobject_id = gameobject_id;
    copy->center = center;
    copy->filter = filter;
    copy->isSensor = isSensor;
    copy->density = density;
    copy->friction = friction;
    copy->bounciness = bounciness;
    copy->rollingResistance = rollingResistance;

    copy->radius = radius;
    copy->height = height;

    copy->state = State::Loaded;
    return copy;

}

CapsuleCollider* CapsuleCollider::Copy(Container* container){
    CapsuleCollider* copy = this->Copy();
    copy->Attach(container);
    return copy;
}

