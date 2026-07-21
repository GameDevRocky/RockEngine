#include "engine/components/ComponentRegistrars.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/ScriptComponent.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/components/Camera.hpp"
#include "engine/components/Animator.hpp"
#include <iostream>


void RegisterComponentTypes() {
    SerializableFactory::RegisterType("Transform", []() { return new Transform(); });
    SerializableFactory::RegisterType("SpriteRenderer", []() { return new SpriteRenderer(); });
    SerializableFactory::RegisterType("ScriptComponent", []() { return new ScriptComponent(); });
    SerializableFactory::RegisterType("RigidBody", []() { return new RigidBody(); });
    SerializableFactory::RegisterType("BoxCollider", []() { return new BoxCollider(); });
    SerializableFactory::RegisterType("CircleCollider", []() { return new CircleCollider(); });
    SerializableFactory::RegisterType("CapsuleCollider", []() { return new CapsuleCollider(); });
    SerializableFactory::RegisterType("Animator", []() { return new Animator(); });
    SerializableFactory::RegisterType("Camera", []() { return new Camera(); });

}
