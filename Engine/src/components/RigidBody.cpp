#include "engine/components/RigidBody.hpp"
#include "engine/core/PhysicsSystem.hpp"
#include "engine/components/Transform.hpp"
#include <iostream>

YAML::Node RigidBody::Serialize(){
    YAML::Node node = Component::Serialize();
    // Add RigidBody-specific serialization here
    return node;
}

void RigidBody::Deserialize(const YAML::Node& node){
    Component::Deserialize(node);


}
void RigidBody::Init(){
    bodyId = b2_nullBodyId;

}

void RigidBody::PostInit(){



}


void RigidBody::Awake(){
    physicsSystem = container->FindSystem<PhysicsSystem>();
    
    Transform* transform = GetTransform();
    
    b2BodyDef definition = b2DefaultBodyDef();
    definition.type = b2BodyType::b2_dynamicBody;
    
    glm::vec2 pos = transform->localPosition;
    definition.position = {pos.x / PTM, pos.y / PTM};
    
    
    bodyId = physicsSystem->CreateRigidBody(definition);
    
    // Create a box shape so the body has mass and can be affected by gravity
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 0.1f;
    
    // Create a 1x1 meter box (will be 32x32 pixels when rendered)
    b2Polygon box = b2MakeBox(0.5f, 0.5f);
    b2CreatePolygonShape(bodyId, &shapeDef, &box);
}


void RigidBody::Update() {
    const float RAD_TO_DEG = 180.0f / 3.1415926535f;
    Transform* transform = GetTransform();
    if (!transform) return;

    if (b2Body_IsValid(bodyId)) {
        b2Vec2 physicsPos = b2Body_GetPosition(bodyId);
        b2Rot physicsRot = b2Body_GetRotation(bodyId);

        float renderX = physicsPos.x * PTM;
        float renderY = physicsPos.y * PTM;

        float renderAngle = b2Rot_GetAngle(physicsRot) * RAD_TO_DEG;

        transform->SetPosition({renderX, renderY});
        transform->SetRotation(renderAngle);
    }
}



RigidBody* RigidBody::Copy(){
    RigidBody* copy = new RigidBody();
    copy->id = id;
    copy->enabled = enabled;
    copy->gameobject_id = gameobject_id;
    copy->bodyId = b2_nullBodyId;
    return copy;
}

RigidBody* RigidBody::Copy(Container* container){
    RigidBody* copy = RigidBody::Copy();
    copy->Attach(container);
    return copy;
}