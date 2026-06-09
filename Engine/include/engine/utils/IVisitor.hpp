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
};