#include "engine/components/CircleCollider.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/ComponentImpl.hpp"
#include "engine/core/GameObjectImpl.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/debug/Console.hpp"

#include <cmath>

using namespace EngineUtils::RenderUtils;

void CircleCollider::Deserialize(const YAML::Node& node){
    if (state >= State::Loaded) return;
    Collider::Deserialize(node);
 
    radius = node["radius"].as<float>();
    state = State::Loaded;
}

YAML::Node CircleCollider::Serialize(){
    YAML::Node node = Collider::Serialize();
    node["type"] = GetTypeName();
    node["radius"] = radius;
    return node;
}

void CircleCollider::PostInit(){
    if (state >= State::PostInitialized) return;
    Collider::PostInit();
    if (radius <= 0.0f) {
        SpriteRenderer* renderer = GetComponent<SpriteRenderer>();
        if (!renderer) {
            renderer = GetComponentInParent<SpriteRenderer>();
        }
        if (renderer && renderer->GetSprite()) {
            glm::vec2 pixelSize = renderer->GetSprite()->GetPixelSize();
            glm::vec2 scaledSize = PixelsToWorld(pixelSize);
            radius = std::max(scaledSize.x, scaledSize.y) / 2;
        } else {
            radius = 1.0f;
            std::cout << "Defaulting Radius to 1.0f \n" << std::endl;
        }
    }
    state = State::PostInitialized;
    
}

void CircleCollider::Awake(){
    
    if (state >= State::Awakened) return;
    
    if (!rigidBody) {
        std::cerr << "CircleCollider::Awake - RigidBody is null on GameObject: " << GetGameObject()->GetName() << std::endl;
        state = State::Awakened;
        return;
    }
    
    b2BodyId bodyId = rigidBody->GetBodyId();

    if (!b2Body_IsValid(bodyId)) {
        std::cerr << "CircleCollider::Start - RigidBody has invalid bodyId on GameObject: " << GetGameObject()->GetName() << std::endl;
        state = State::Awakened;
        return;
    }
    CreateShape();
    state = State::Awakened;
}

void CircleCollider::SetRadius(float radius){
    this->radius = radius;
    CreateShape();
    this->Notify(CircleCollider::CHANGED_EVENT);
}


void CircleCollider::CreateShape(){
    if (!rigidBody) {
        std::cerr << "CircleCollider::CreateShape - RigidBody is null" << std::endl;
        return;
    }
    
    Transform* transform = GetTransform();
    if (!transform) {
        std::cerr << "CircleCollider::CreateShape - Transform is null" << std::endl;
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
    float averageScale = std::max(std::abs(worldScale.x), std::abs(worldScale.y));  // Use absolute scale for physics
    float scaledRadius = radius * averageScale;
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
    
    float physicsRadius = scaledRadius / PixelsPerUnit;

    b2Circle circle;
    circle.center = physicsCenter;
    circle.radius = physicsRadius;

    if (b2Shape_IsValid(shapeId)) b2DestroyShape(shapeId, true);    
    shapeId = b2CreateCircleShape(bodyId, &definition, &circle);
    b2Body_ApplyMassFromShapes(bodyId);
    
}

void CircleCollider::Accept(IVisitor* v) {
    
    v->Visit(this); 
}

CircleCollider* CircleCollider::Copy(){
    
    CircleCollider* copy = new CircleCollider();
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

    copy->state = State::Loaded;
    return copy;
}

CircleCollider* CircleCollider::Copy(Container* container){
    CircleCollider* copy = this->Copy();
    copy->Attach(container);
    return copy;
}