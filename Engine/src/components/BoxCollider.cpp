#include "engine/components/BoxCollider.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/ComponentImpl.hpp"
#include "engine/core/GameObjectImpl.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/debug/Console.hpp"

using namespace EngineUtils::RenderUtils;
YAML::Node BoxCollider::Serialize(){
    YAML::Node node = Component::Serialize();
    return node;
}

void BoxCollider::Deserialize(const YAML::Node& node){
    if (state >= State::Loaded) return;
    Collider::Deserialize(node);
    
    float w = node["size"][0].as<float>();
    float h = node["size"][1].as<float> ();
    
    size = {w, h};
    state = State::Loaded;
}

void BoxCollider::PostInit(){
    if (state >= State::PostInitialized) return;
    Collider::PostInit();
    if (size.x <= 0.0f || size.y <= 0.0f) {
        SpriteRenderer* renderer = GetComponent<SpriteRenderer>();
        if (!renderer) {
            renderer = GetComponentInParent<SpriteRenderer>();
        }
        if (renderer && renderer->GetSprite()) {
            glm::vec2 pixelSize = renderer->GetSprite()->GetPixelSize();
            size = PixelsToWorld(pixelSize);
        } else {
            size = glm::vec2(1.0f, 1.0f);
        }
    }
    state = State::PostInitialized;
    
}

void BoxCollider::Awake(){
    
    if (state >= State::Awakened) return;
    
    if (!rigidBody) {
        std::cerr << "BoxCollider::Awake - RigidBody is null on GameObject: " << GetGameObject()->GetName() << std::endl;
        state = State::Awakened;
        return;
    }
    
    b2BodyId bodyId = rigidBody->GetBodyId();

    if (!b2Body_IsValid(bodyId)) {
        std::cerr << "BoxCollider::Start - RigidBody has invalid bodyId on GameObject: " << GetGameObject()->GetName() << std::endl;
        state = State::Awakened;
        return;
    }
    CreateShape();
    state = State::Awakened;
}

void BoxCollider::SetSize(glm::vec2 size){
    this->size = size;
    CreateShape();
}


void BoxCollider::CreateShape(){
    if (!rigidBody) {
        std::cerr << "BoxCollider::CreateShape - RigidBody is null" << std::endl;
        return;
    }
    
    Transform* transform = GetTransform();
    if (!transform) {
        std::cerr << "BoxCollider::CreateShape - Transform is null" << std::endl;
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
    glm::vec2 scaledSize = size * worldScale;
    b2Vec2 physicsCenter = {center.x * worldScale.x / PixelsPerUnit , center.y * worldScale.x / PixelsPerUnit};
    
    float physicsHalfWidth = scaledSize.x / (2.0f * PixelsPerUnit);
    float physicsHalfHeight = scaledSize.y / (2.0f * PixelsPerUnit);

    b2Polygon box = b2MakeOffsetBox(physicsHalfWidth, physicsHalfHeight, physicsCenter, b2Rot_identity);

    if (b2Shape_IsValid(shapeId)) b2DestroyShape(shapeId, true);    
    shapeId = b2CreatePolygonShape(bodyId, &definition, &box);
    b2Body_ApplyMassFromShapes(bodyId);
    
}


BoxCollider* BoxCollider::Copy(){
    
    BoxCollider* copy = new BoxCollider();
    copy->id = id;
    copy->gameobject_id = gameobject_id;
    copy->center = center;
    copy->filter = filter;
    copy->isSensor = isSensor;
    copy->density = density;
    copy->friction = friction;
    copy->bounciness = bounciness;
    copy->rollingResistance = rollingResistance;

    copy->size = size;

    copy->state = State::Loaded;
    return copy;
}

BoxCollider* BoxCollider::Copy(Container* container){
    BoxCollider* copy = this->Copy();
    copy->Attach(container);
    return copy;
}