#pragma once
#include "engine/core/System.hpp"
#include <box2d/box2d.h>

class TimeManager;

class PhysicsSystem : public System{
public:
    void Init() override;
    void Shutdown() override;
    void Step();
    void DestroyBody(b2BodyId bodyId);
    void DestroyShape(b2ShapeId bodyId);
    b2RayResult CastRay(b2Vec2 origin, b2Vec2 translation, b2QueryFilter filter);

    b2WorldId GetWorldId(){return worldId;}

    PhysicsSystem* Copy() override;
    PhysicsSystem* Copy(Container* container) override;

private:
    b2WorldId worldId = b2_nullWorldId; 
    TimeManager* timeManager;


};
