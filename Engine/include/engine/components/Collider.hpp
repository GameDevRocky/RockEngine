#pragma once
#include "engine/components/Component.hpp"
#include "glm/glm.hpp"
#include "box2d/box2d.h"
class RigidBody;

class Collider : public Component{

public:

    static inline const Event CHANGED_EVENT = Collider::CreateEvent();
    static inline const Event DENSITY_CHANGED_EVENT = Collider::CreateEvent();
    static inline const Event BOUNCINESS_CHANGED_EVENT = Collider::CreateEvent();
    static inline const Event FRICTION_CHANGED_EVENT = Collider::CreateEvent();
    static inline const Event ROLLING_RESISTANCE_CHANGED_EVENT = Collider::CreateEvent();
    static inline const Event IS_SENSOR_CHANGED_EVENT = Collider::CreateEvent();
    static inline const Event CENTER_CHANGED_EVENT = Collider::CreateEvent();

    void Deserialize(const YAML::Node& node);
    virtual void Init() override;
    virtual void PostInit() override;
    
    virtual void CreateShape(){};
    

    virtual void SetCenter(glm::vec2 center);
    virtual void SetDensity(float density);
    virtual void SetBounciness(float bounciness);
    virtual void SetIsSensor(bool val);
    virtual void SetFriction(float friction);
    virtual void SetRollingResistance(float rollingResistance);
    virtual void OnEnabled() override;
    virtual void OnDisabled() override;
    virtual void OnTransformScaleUpdate();

    glm::vec2 GetCenter(){return center;}
    float GetDensity(){return density;}
    float GetBounciness(){return bounciness;}
    bool GetIsSensor(){return isSensor;}
    float GetFriction(){return friction;}
    float GetRollingResistance(){return rollingResistance;}
    void Accept(IVisitor* v) override;
    
    std::string GetTypeName() const override {return "Collider";}

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

    glm::vec2 cachedScale = {1, 1};
};