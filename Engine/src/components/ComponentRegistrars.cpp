#include "engine/components/ComponentRegistrars.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/TextRenderer.hpp"
#include "engine/components/ScriptComponent.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/components/Camera.hpp"
#include "engine/components/Animator.hpp"
#include "engine/components/ParticleComponent.hpp"
#include "engine/components/Light.hpp"
#include "engine/components/ShadowCaster.hpp"
#include "engine/components/DistanceJoint.hpp"
#include "engine/components/RevoluteJoint.hpp"
#include "engine/components/PrismaticJoint.hpp"
#include "engine/components/WeldJoint.hpp"
#include "engine/components/WheelJoint.hpp"
#include "engine/components/MotorJoint.hpp"
#include "engine/components/AudioSource.hpp"
#include "engine/components/AudioListener.hpp"
#include <iostream>


void RegisterComponentTypes() {
    SerializableFactory::RegisterType("Transform", []() { return new Transform(); });
    SerializableFactory::RegisterType("SpriteRenderer", []() { return new SpriteRenderer(); });
    SerializableFactory::RegisterType("TextRenderer", []() { return new TextRenderer(); });
    SerializableFactory::RegisterType("ScriptComponent", []() { return new ScriptComponent(); });
    SerializableFactory::RegisterType("RigidBody", []() { return new RigidBody(); });
    SerializableFactory::RegisterType("BoxCollider", []() { return new BoxCollider(); });
    SerializableFactory::RegisterType("CircleCollider", []() { return new CircleCollider(); });
    SerializableFactory::RegisterType("CapsuleCollider", []() { return new CapsuleCollider(); });
    SerializableFactory::RegisterType("Animator", []() { return new Animator(); });
    SerializableFactory::RegisterType("Camera", []() { return new Camera(); });
    SerializableFactory::RegisterType("ParticleComponent", []() { return new ParticleComponent(); });
    SerializableFactory::RegisterType("Light", []() { return new Light(); });
    SerializableFactory::RegisterType("ShadowCaster", []() { return new ShadowCaster(); });
    // Joints are intentionally NOT in Component::IsSingleton -- one body may be
    // constrained to several others at once.
    SerializableFactory::RegisterType("DistanceJoint", []() { return new DistanceJoint(); });
    SerializableFactory::RegisterType("RevoluteJoint", []() { return new RevoluteJoint(); });
    SerializableFactory::RegisterType("PrismaticJoint", []() { return new PrismaticJoint(); });
    SerializableFactory::RegisterType("WeldJoint", []() { return new WeldJoint(); });
    SerializableFactory::RegisterType("WheelJoint", []() { return new WheelJoint(); });
    SerializableFactory::RegisterType("MotorJoint", []() { return new MotorJoint(); });
    SerializableFactory::RegisterType("AudioSource", []() { return new AudioSource(); });
    SerializableFactory::RegisterType("AudioListener", []() { return new AudioListener(); });

}
