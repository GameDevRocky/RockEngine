#pragma once
#include "engine/utils/Properties.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/core/Observable.hpp"

using namespace Properties;

class Serializable;
class Transform;
class Rigidbody;
class BoxCollider;
class CircleCollider;
class CapsuleCollider;
class SpriteRenderer;
class ScriptComponent;
class GameObject;
class Collider;

class IVisitor {
public:
    virtual ~IVisitor() = default;
    virtual void Visit(Serializable* s) {}
    virtual void Visit(GameObject*  obj) {}
    virtual void Visit(Transform* t) {}
    virtual void Visit(Rigidbody* rb) {}
    virtual void Visit(Collider* bc) {}
    virtual void Visit(BoxCollider* bc) {}
    virtual void Visit(CircleCollider* cc) {}
    virtual void Visit(CapsuleCollider* cac) {}
    virtual void Visit(SpriteRenderer* sr) {}
    virtual void Visit(ScriptComponent* sc) {}
};