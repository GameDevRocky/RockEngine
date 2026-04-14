#include "engine/bindings/PythonBindings.hpp"
#include "engine/core/InputManager.hpp"
#include "engine/components/RigidBody.hpp"
#include <iostream>
#include "Engine.hpp"


void BindRigidBody(pybind11::module_& m) {
    pybind11::module_ rigidbody_module = m.def_submodule("rigidbody_module", "RigidBody");
    
    rigidbody_module.def("apply_force", [](const std::string& id, float x, float y) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        auto* rb = go->GetComponent<RigidBody>();
        if (rb) {
            rb->ApplyForceToCenter({x, y});
        }
    });
    rigidbody_module.def("apply_impulse", [](const std::string& id, float x, float y) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        auto* rb = go->GetComponent<RigidBody>();
        if (rb) {
            rb->ApplyLinearImpulse({x, y});
        }
    });
    rigidbody_module.def("set_velocity", [](const std::string& id, float x, float y) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        auto* rb = go->GetComponent<RigidBody>();
        if (rb) {
            rb->SetLinearVelocity({x, y});
        }
    });
    rigidbody_module.def("get_velocity", [](const std::string& id) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        auto* rb = go->GetComponent<RigidBody>();
        if (rb) {
            auto vel = rb->GetLinearVelocity();
            
            return std::make_tuple(vel.x, vel.y);
        }
    });



}