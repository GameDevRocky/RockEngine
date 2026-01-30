#pragma once
#include "engine/core/System.hpp"
#include <box2d/box2d.h>

class PhysicsSystem : public System{
public:
    void Init() override;
    void PostInit() override;
    void Step();
    void Update() override;
    void Shutdown() override;
    void OnEnterPlayMode() override;
    void OnExitPlayMode() override;
    PhysicsSystem* Copy() override;
    PhysicsSystem* Copy(Container* container) override;

private:
    b2WorldId worldId; 

};
