#include "engine/core/PhysicsSystem.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/debug/Console.hpp"
#include <iostream>
void PhysicsSystem::Init(){
    timeManager = container->FindSystem<TimeManager>();
    
    b2WorldDef worldDef = b2DefaultWorldDef();
	worldDef.enableContinuous = true;
    worldDef.enableSleep = false;
    worldDef.gravity = {0.0f, -9.8f};
    worldDef.gravity *= 1.0f; 
    worldId = b2CreateWorld(&worldDef);
    std::cout << "PhysicsSystem: World created, valid=" << B2_IS_NON_NULL(worldId) << std::endl;
}


void PhysicsSystem::Step() {
    if (container->GetMode() == Container::Mode::Editor) return;
    if (!B2_IS_NON_NULL(worldId)) return;

    b2World_Step(worldId, timeManager->FixedDeltaTime(), 4);

    b2ContactEvents contactEvents = b2World_GetContactEvents(worldId);

    for (int i = 0; i < contactEvents.beginCount; ++i) {
        b2ContactBeginTouchEvent* event = contactEvents.beginEvents + i;
        if (!b2Shape_IsValid(event->shapeIdA) || !b2Shape_IsValid(event->shapeIdB)) continue;

        b2BodyId bodyA = b2Shape_GetBody(event->shapeIdA);
        b2BodyId bodyB = b2Shape_GetBody(event->shapeIdB);

        GameObject* objA = static_cast<GameObject*>(b2Body_GetUserData(bodyA));
        GameObject* objB = static_cast<GameObject*>(b2Body_GetUserData(bodyB));

        if (objA && objB) {
            objA->OnCollisionEnter(objB);
            objB->OnCollisionEnter(objA);
        }
    }

    for (int i = 0; i < contactEvents.endCount; ++i) {
        b2ContactEndTouchEvent* event = contactEvents.endEvents + i;

        if (!b2Shape_IsValid(event->shapeIdA) || !b2Shape_IsValid(event->shapeIdB)) continue;

        b2BodyId bodyA = b2Shape_GetBody(event->shapeIdA);
        b2BodyId bodyB = b2Shape_GetBody(event->shapeIdB);

        GameObject* objA = static_cast<GameObject*>(b2Body_GetUserData(bodyA));
        GameObject* objB = static_cast<GameObject*>(b2Body_GetUserData(bodyB));

        if (objA && objB) {
            objA->OnCollisionExit(objB);
            objB->OnCollisionExit(objA);
        }
    }

    b2SensorEvents sensorEvents = b2World_GetSensorEvents(worldId);

    for (int i = 0; i < sensorEvents.beginCount; ++i) {
        b2SensorBeginTouchEvent* event = sensorEvents.beginEvents + i;

        // Skip if shapes were destroyed (e.g., during scale change)
        if (!b2Shape_IsValid(event->sensorShapeId) || !b2Shape_IsValid(event->visitorShapeId)) continue;

        b2BodyId sensorBody = b2Shape_GetBody(event->sensorShapeId);
        b2BodyId visitorBody = b2Shape_GetBody(event->visitorShapeId);

        GameObject* sensorObj = static_cast<GameObject*>(b2Body_GetUserData(sensorBody));
        GameObject* visitorObj = static_cast<GameObject*>(b2Body_GetUserData(visitorBody));

        if (sensorObj && visitorObj) {
            sensorObj->OnTriggerEnter(visitorObj);
            visitorObj->OnTriggerEnter(sensorObj);
        }
    }

    for (int i = 0; i < sensorEvents.endCount; ++i) {
        b2SensorEndTouchEvent* event = sensorEvents.endEvents + i;

        // Skip if shapes were destroyed (e.g., during scale change)
        if (!b2Shape_IsValid(event->sensorShapeId) || !b2Shape_IsValid(event->visitorShapeId)) continue;

        b2BodyId sensorBody = b2Shape_GetBody(event->sensorShapeId);
        b2BodyId visitorBody = b2Shape_GetBody(event->visitorShapeId);

        GameObject* sensorObj = static_cast<GameObject*>(b2Body_GetUserData(sensorBody));
        GameObject* visitorObj = static_cast<GameObject*>(b2Body_GetUserData(visitorBody));

        if (sensorObj && visitorObj) {
            sensorObj->OnTriggerExit(visitorObj);
            visitorObj->OnTriggerExit(sensorObj);
        }
    }
}

void PhysicsSystem::Shutdown(){
    if (B2_IS_NON_NULL(worldId)) {
        b2DestroyWorld(worldId);
        worldId = b2_nullWorldId;
    }
}

PhysicsSystem* PhysicsSystem::Copy(){
    PhysicsSystem* copy = new PhysicsSystem();

    copy->worldId = b2_nullWorldId;
    return copy;
}
PhysicsSystem* PhysicsSystem::Copy(Container* container){
    PhysicsSystem* copy = this->Copy();
    copy->Attach(container);
    return copy;
}