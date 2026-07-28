#pragma once
#include "engine/utils/Properties.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/core/Observable.hpp"

using namespace Properties;

class Serializable;
class Transform;
class BoxCollider;
class CircleCollider;
class CapsuleCollider;
class SpriteRenderer;
class ScriptComponent;
class GameObject;
class Collider;
class RigidBody;
class Sprite;
class Material;
class Texture2D;
class Shader;
class Camera;
class Animator;
class ParticleComponent;
class Light;
class ShadowCaster;
class Joint;
class DistanceJoint;
class RevoluteJoint;
class PrismaticJoint;
class WeldJoint;
class WheelJoint;
class MotorJoint;

class IVisitor {
public:
    virtual ~IVisitor() = default;
    virtual void Visit(Serializable* s) {}
    virtual void Visit(GameObject*  obj) {}
    virtual void Visit(Transform* t) {}
    virtual void Visit(RigidBody* rb) {}
    virtual void Visit(Collider* bc) {}
    virtual void Visit(BoxCollider* bc) {}
    virtual void Visit(CircleCollider* cc) {}
    virtual void Visit(CapsuleCollider* cac) {}
    virtual void Visit(SpriteRenderer* sr) {}
    virtual void Visit(ScriptComponent* sc) {}
    virtual void Visit(Sprite* s) {}
    virtual void Visit(Material* m) {}
    virtual void Visit(Texture2D* t) {}
    virtual void Visit(Shader* s) {}
    virtual void Visit(Camera* c) {}
    virtual void Visit(Animator* a) {}
    virtual void Visit(ParticleComponent* p) {}
    virtual void Visit(Light* l) {}
    virtual void Visit(ShadowCaster* sc) {}
    virtual void Visit(Joint* j) {}
    virtual void Visit(DistanceJoint* j) {}
    virtual void Visit(RevoluteJoint* j) {}
    virtual void Visit(PrismaticJoint* j) {}
    virtual void Visit(WeldJoint* j) {}
    virtual void Visit(WheelJoint* j) {}
    virtual void Visit(MotorJoint* j) {}
};