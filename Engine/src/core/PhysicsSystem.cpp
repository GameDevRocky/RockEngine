#include "engine/core/PhysicsSystem.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/TimeManager.hpp"
void PhysicsSystem::Init(){
    worldId = b2_nullWorldId;
}

void PhysicsSystem::PostInit(){

}
void PhysicsSystem::OnEnterPlayMode(){
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = {0.0f, -10.0f}; 
    worldId = b2CreateWorld(&worldDef);
}

void PhysicsSystem::OnExitPlayMode(){
    if (B2_IS_NON_NULL(worldId)) {
        Shutdown();
    }
}

void PhysicsSystem::Update(){
}

void PhysicsSystem::Step(){
    TimeManager* timeManager = container->FindSystem<TimeManager>();
    if (B2_IS_NON_NULL(worldId)) {
        float deltaTime = timeManager->DeltaTime();
        b2World_Step(worldId, deltaTime, 4);
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