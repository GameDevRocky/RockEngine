#include "engine/bindings/PythonBindings.hpp"
#include "engine/core/InputManager.hpp"
#include "engine/components/RigidBody.hpp"
#include <iostream>
#include "Engine.hpp"


void BindRigidBody(pybind11::module_& m) {
    pybind11::module_ rigidbody_module = m.def_submodule("rigidbody_module", "RigidBody");
    
    rigidbody_module.def("apply_force", [](const std::string& id, float x, float y) {
        GameObject* go = registry->Find<GameObject>(id);
        if (!go) return;
        auto* rb = go->GetComponent<RigidBody>();
        if (rb) {
            rb->ApplyForceToCenter({x, y});
        }
    });
    rigidbody_module.def("apply_impulse", [](const std::string& id, float x, float y) {
        GameObject* go = registry->Find<GameObject>(id);
        if (!go) return;
        auto* rb = go->GetComponent<RigidBody>();
        if (rb) {
            rb->ApplyLinearImpulse({x, y});
        }
    });

    rigidbody_module.def("apply_torque", [](const std::string& id, float torque) {
        GameObject* go = registry->Find<GameObject>(id);
        if (!go) return;
        auto* rb = go->GetComponent<RigidBody>();
        if (rb) {
            rb->ApplyTorque(torque);
        }
    });

    rigidbody_module.def("apply_angular_impulse", [](const std::string& id, float impulse) {
        GameObject* go = registry->Find<GameObject>(id);
        if (!go) return;
        auto* rb = go->GetComponent<RigidBody>();
        if (rb) {
            rb->ApplyAngularImpulse(impulse);
        }
    });

    rigidbody_module.def("set_velocity", [](const std::string& id, float x, float y) {
        GameObject* go = registry->Find<GameObject>(id);
        if (!go) return;
        auto* rb = go->GetComponent<RigidBody>();
        if (rb) {
            rb->SetLinearVelocity({x, y});
        }
    });
    rigidbody_module.def("get_velocity", [](const std::string& id) {
        GameObject* go = registry->Find<GameObject>(id);
        if (!go) return std::make_tuple(0.0f, 0.0f);
        auto* rb = go->GetComponent<RigidBody>();
        if (rb) {
            auto vel = rb->GetLinearVelocity();
            return std::make_tuple(vel.x, vel.y);
        }
        return std::make_tuple(0.0f, 0.0f);
    });

    rigidbody_module.def("set_use_gravity", [](const std::string& id, bool val) {
        GameObject* go = registry->Find<GameObject>(id);
        if (!go) return;
        auto* rb = go->GetComponent<RigidBody>();
        if (!rb) return;
        rb->SetUseGravity(val);
    });

    rigidbody_module.def("get_use_gravity", [](const std::string& id) {
        GameObject* go = registry->Find<GameObject>(id);
        if (!go) return false;
        auto* rb = go->GetComponent<RigidBody>();
        if (!rb) return false;
        return rb->GetUseGravity();
    });

    rigidbody_module.def("set_body_type", [](const std::string& id, const std::string& type) {
        GameObject* go = registry->Find<GameObject>(id);
        if (!go) return;
        auto* rb = go->GetComponent<RigidBody>();
        if (!rb) return;
        if      (type == "Dynamic")   rb->SetBodyType(b2_dynamicBody);
        else if (type == "Kinematic") rb->SetBodyType(b2_kinematicBody);
        else if (type == "Static")    rb->SetBodyType(b2_staticBody);
    });

    rigidbody_module.def("get_body_type", [](const std::string& id) -> std::string {
        GameObject* go = registry->Find<GameObject>(id);
        if (!go) return "Dynamic";
        auto* rb = go->GetComponent<RigidBody>();
        if (!rb) return "Dynamic";
        switch (rb->GetBodyType()) {
            case b2_kinematicBody: return "Kinematic";
            case b2_staticBody:    return "Static";
            default:               return "Dynamic";
        }
    });

    rigidbody_module.def("set_lock_rotation", [](const std::string& id, bool val) {
        GameObject* go = registry->Find<GameObject>(id);
        if (!go) return;
        auto* rb = go->GetComponent<RigidBody>();
        if (!rb) return;
        rb->SetLockRotation(val);
    });

    rigidbody_module.def("get_lock_rotation", [](const std::string& id) {
        GameObject* go = registry->Find<GameObject>(id);
        if (!go) return false;
        auto* rb = go->GetComponent<RigidBody>();
        if (!rb) return false;
        return rb->GetLockRotation();
    });

}