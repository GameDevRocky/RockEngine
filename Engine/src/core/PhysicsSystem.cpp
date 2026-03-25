#include "engine/core/PhysicsSystem.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/TimeManager.hpp"
#include <iostream>
void PhysicsSystem::Init(){
    timeManager = container->FindSystem<TimeManager>();
    
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = {0.0f, -9.8f};
    worldDef.gravity *= 1.0f; 
    worldId = b2CreateWorld(&worldDef);
    std::cout << "PhysicsSystem: World created, valid=" << B2_IS_NON_NULL(worldId) << std::endl;
}


void PhysicsSystem::Step(){
    if (container->GetMode() == Container::Mode::Editor) return;
    
    if (B2_IS_NON_NULL(worldId)) {
        float fixedDeltaTime = timeManager->FixedDeltaTime();
        b2World_Step(worldId, fixedDeltaTime, 4);
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