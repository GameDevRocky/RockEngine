#pragma once
#include "engine/components/Component.hpp"
#include "glm/glm.hpp"
#include "box2d/box2d.h"
class RigidBody;

class Collider : public Component{

public:
    void Deserialize(const YAML::Node& node);
    virtual void Init() override;
    
    virtual void CreateShape(){};
    

    virtual void SetCenter(glm::vec2 center);
    virtual void SetDensity(float density);
    virtual void SetBounciness(float bounciness);
    virtual void SetIsSensor(bool val);
    virtual void SetFriction(float friction);
    virtual void SetRollingResistance(float rollingResistance);

    glm::vec2 GetCenter(){return center;}
    float GetDensity(){return density;}
    float GetBounciness(){return bounciness;}
    bool GetIsSensor(){return isSensor;}
    float GetFriction(){return friction;}

    Collider() = default;

protected:
    RigidBody* rigidBody = nullptr;
    b2ShapeId shapeId;
    glm::vec2 center = {0, 0};
    b2Filter filter = b2DefaultFilter();
    bool isSensor = false;
    float density = 1.0f;
    float friction = 0.0f;
    float bounciness = 0.0f;
    float rollingResistance = 0.0f;
};