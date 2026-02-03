#include "engine/components/BoxCollider.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/ComponentImpl.hpp"
#include "engine/core/GameObjectImpl.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils::RenderUtils;
YAML::Node BoxCollider::Serialize(){
    YAML::Node node = Component::Serialize();
    return node;
}

void BoxCollider::Deserialize(const YAML::Node& node){
    Component::Deserialize(node);
    isSensor = node["isSensor"].as<bool>();
    float x = node["center"][0].as<float>();
    float y = node["center"][1].as<float> ();
    float w = node["size"][0].as<float>();
    float h = node["size"][1].as<float> ();
    center = {x, y};
    size = {w, h};
}

void BoxCollider::Init(){




}
void BoxCollider::PostInit(){

    if (size.x <= 0.0f || size.y <= 0.0f) {
        SpriteRenderer* renderer = GetComponent<SpriteRenderer>();
        if (!renderer) {
            renderer = GetComponentInParent<SpriteRenderer>();
        }
        
        if (renderer && renderer->GetSprite()) {
            size = PixelsToWorld(renderer->GetSprite()->GetPixelSize());
        } else {
            size = glm::vec2(1.0f, 1.0f);
        }
    }
    
}

void BoxCollider::Awake(){

    b2BodyId bodyId;
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.isSensor = isSensor;
   
    Transform* transform = GetTransform();
    glm::vec2 worldScale = transform->GetWorldScale();
    glm::vec2 scaledSize = size * worldScale;
    
    b2Vec2 physicsCenter = {center.x / PixelsPerUnit, center.y / PixelsPerUnit};
    float physicsHalfWidth = scaledSize.x / (2.0f * PixelsPerUnit);
    float physicsHalfHeight = scaledSize.y / (2.0f * PixelsPerUnit);
    
    b2Polygon dynamicBox = b2MakeOffsetBox(physicsHalfWidth, physicsHalfHeight, physicsCenter, b2Rot_identity);

    RigidBody* rigidBody;
    if (GetComponent<RigidBody>()){
        rigidBody = GetComponent<RigidBody>();
    }
    else if (GetComponentInParent<RigidBody>()) {
        rigidBody = GetComponentInParent<RigidBody>();
    }
    bodyId = rigidBody->GetBodyId();
    shapeId = b2CreatePolygonShape(bodyId, &shapeDef, &dynamicBox);
    b2Shape_SetRestitution(shapeId, 0.1);
}

void BoxCollider::Update(){

}

void BoxCollider::OnDestroy(){

}

void BoxCollider::SetIsSensor(bool val){
    isSensor = val;
}

void BoxCollider::SetSize(glm::vec2 size){
    this->size = size;
}
void BoxCollider::SetCenter(glm::vec2 center){
    this->center = center;
}


BoxCollider* BoxCollider::Copy(){
    BoxCollider* copy = new BoxCollider();

    copy->id = id;
    copy->gameobject_id = gameobject_id;
    copy->isSensor = isSensor;
    copy->center = center;
    copy->size = size;
    return copy;
}

BoxCollider* BoxCollider::Copy(Container* container){
    BoxCollider* copy = this->Copy();
    copy->Attach(container);
    return copy;
}