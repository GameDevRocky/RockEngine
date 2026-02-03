#pragma once
#include "engine/core/System.hpp"
#include <box2d/box2d.h>

class TimeManager;

class PhysicsSystem : public System{
public:
    void Init() override;
    void PostInit() override;
    void Step();
    void Update() override;
    void Shutdown() override;
    void OnEnterPlayMode() override;
    void OnExitPlayMode() override;

    b2WorldId GetWorldId(){return worldId;}

    PhysicsSystem* Copy() override;
    PhysicsSystem* Copy(Container* container) override;

private:
    b2WorldId worldId = b2_nullWorldId; 
    TimeManager* timeManager;

};
