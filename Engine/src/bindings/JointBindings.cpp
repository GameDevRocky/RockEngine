#include "engine/bindings/PythonBindings.hpp"

#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Joint.hpp"
#include "engine/components/DistanceJoint.hpp"
#include "engine/components/RevoluteJoint.hpp"
#include "engine/components/PrismaticJoint.hpp"
#include "engine/components/WeldJoint.hpp"
#include "engine/components/WheelJoint.hpp"
#include "engine/components/MotorJoint.hpp"

#include <pybind11/stl.h>

// Joint bindings are keyed by the COMPONENT id, not the GameObject id that
// RigidBody/Collider bindings use. A body may carry several joints at once, so a
// GameObject id cannot name which one you mean -- base_component_module already
// addresses components this way for the same reason.
namespace {
    template<typename T>
    T* Find(const std::string& id) { return registry->Find<T>(id); }
}

// Cuts the getter/setter boilerplate down to the part that actually differs: the
// component type, the python-side name, and the accessor pair. An id that does
// not resolve yields a zero/false default rather than throwing into script code,
// matching how every other binding in this directory degrades.
//
// Lambdas must have concrete parameter types -- pybind11 cannot introspect a
// generic (auto) lambda, since that is a template, not a single signature.
#define JOINT_FLOAT(mod, Type, pyname, Getter, Setter)                                      \
    mod.def("get_" pyname, [](const std::string& id) -> float {                             \
        Type* j = Find<Type>(id); return j ? j->Getter() : 0.0f; });                        \
    mod.def("set_" pyname, [](const std::string& id, float value) {                         \
        if (Type* j = Find<Type>(id)) j->Setter(value); })

#define JOINT_BOOL(mod, Type, pyname, Getter, Setter)                                       \
    mod.def("get_" pyname, [](const std::string& id) -> bool {                              \
        Type* j = Find<Type>(id); return j ? j->Getter() : false; });                       \
    mod.def("set_" pyname, [](const std::string& id, bool value) {                          \
        if (Type* j = Find<Type>(id)) j->Setter(value); })

// Read-only telemetry pulled straight from the live Box2D joint.
#define JOINT_FLOAT_RO(mod, Type, pyname, Getter)                                           \
    mod.def("get_" pyname, [](const std::string& id) -> float {                             \
        Type* j = Find<Type>(id); return j ? j->Getter() : 0.0f; })

void BindJoint(pybind11::module_& m) {

    // ─── Shared across every joint type ──────────────────────────────────────
    pybind11::module_ joint_module = m.def_submodule("joint_module", "Joint Bindings");

    joint_module.def("get_connected_body", [](const std::string& id) {
        Joint* j = Find<Joint>(id);
        return j ? j->GetConnectedBody() : std::string("");
    });
    joint_module.def("set_connected_body", [](const std::string& id, const std::string& go_id) {
        if (Joint* j = Find<Joint>(id)) j->SetConnectedBody(go_id);
    });
    joint_module.def("get_collide_connected", [](const std::string& id) {
        Joint* j = Find<Joint>(id);
        return j ? j->GetCollideConnected() : false;
    });
    joint_module.def("set_collide_connected", [](const std::string& id, bool val) {
        if (Joint* j = Find<Joint>(id)) j->SetCollideConnected(val);
    });
    joint_module.def("get_local_anchor_a", [](const std::string& id) {
        Joint* j = Find<Joint>(id);
        glm::vec2 a = j ? j->GetLocalAnchorA() : glm::vec2(0.0f);
        return std::make_tuple(a.x, a.y);
    });
    joint_module.def("set_local_anchor_a", [](const std::string& id, float x, float y) {
        if (Joint* j = Find<Joint>(id)) j->SetLocalAnchorA({x, y});
    });
    joint_module.def("get_local_anchor_b", [](const std::string& id) {
        Joint* j = Find<Joint>(id);
        glm::vec2 a = j ? j->GetLocalAnchorB() : glm::vec2(0.0f);
        return std::make_tuple(a.x, a.y);
    });
    joint_module.def("set_local_anchor_b", [](const std::string& id, float x, float y) {
        if (Joint* j = Find<Joint>(id)) j->SetLocalAnchorB({x, y});
    });
    // True once both bodies resolved and the Box2D joint actually exists -- lets a
    // script tell "misconfigured" apart from "configured but not yet in play mode".
    joint_module.def("is_built", [](const std::string& id) {
        Joint* j = Find<Joint>(id);
        return j ? j->IsBuilt() : false;
    });

    // ─── Distance ────────────────────────────────────────────────────────────
    pybind11::module_ distance_module = m.def_submodule("distance_joint_module", "DistanceJoint Bindings");
    JOINT_FLOAT(distance_module, DistanceJoint, "length", GetLength, SetLength);
    JOINT_BOOL (distance_module, DistanceJoint, "enable_spring", GetEnableSpring, SetEnableSpring);
    JOINT_FLOAT(distance_module, DistanceJoint, "hertz", GetHertz, SetHertz);
    JOINT_FLOAT(distance_module, DistanceJoint, "damping_ratio", GetDampingRatio, SetDampingRatio);
    JOINT_FLOAT(distance_module, DistanceJoint, "lower_spring_force", GetLowerSpringForce, SetLowerSpringForce);
    JOINT_FLOAT(distance_module, DistanceJoint, "upper_spring_force", GetUpperSpringForce, SetUpperSpringForce);
    JOINT_BOOL (distance_module, DistanceJoint, "enable_limit", GetEnableLimit, SetEnableLimit);
    JOINT_FLOAT(distance_module, DistanceJoint, "min_length", GetMinLength, SetMinLength);
    JOINT_FLOAT(distance_module, DistanceJoint, "max_length", GetMaxLength, SetMaxLength);
    JOINT_BOOL (distance_module, DistanceJoint, "enable_motor", GetEnableMotor, SetEnableMotor);
    JOINT_FLOAT(distance_module, DistanceJoint, "motor_speed", GetMotorSpeed, SetMotorSpeed);
    JOINT_FLOAT(distance_module, DistanceJoint, "max_motor_force", GetMaxMotorForce, SetMaxMotorForce);
    JOINT_FLOAT_RO(distance_module, DistanceJoint, "current_length", GetCurrentLength);

    // ─── Revolute ────────────────────────────────────────────────────────────
    pybind11::module_ revolute_module = m.def_submodule("revolute_joint_module", "RevoluteJoint Bindings");
    JOINT_FLOAT(revolute_module, RevoluteJoint, "target_angle", GetTargetAngle, SetTargetAngle);
    JOINT_BOOL (revolute_module, RevoluteJoint, "enable_spring", GetEnableSpring, SetEnableSpring);
    JOINT_FLOAT(revolute_module, RevoluteJoint, "hertz", GetHertz, SetHertz);
    JOINT_FLOAT(revolute_module, RevoluteJoint, "damping_ratio", GetDampingRatio, SetDampingRatio);
    JOINT_BOOL (revolute_module, RevoluteJoint, "enable_limit", GetEnableLimit, SetEnableLimit);
    JOINT_FLOAT(revolute_module, RevoluteJoint, "lower_angle", GetLowerAngle, SetLowerAngle);
    JOINT_FLOAT(revolute_module, RevoluteJoint, "upper_angle", GetUpperAngle, SetUpperAngle);
    JOINT_BOOL (revolute_module, RevoluteJoint, "enable_motor", GetEnableMotor, SetEnableMotor);
    JOINT_FLOAT(revolute_module, RevoluteJoint, "motor_speed", GetMotorSpeed, SetMotorSpeed);
    JOINT_FLOAT(revolute_module, RevoluteJoint, "max_motor_torque", GetMaxMotorTorque, SetMaxMotorTorque);
    JOINT_FLOAT_RO(revolute_module, RevoluteJoint, "angle", GetAngle);

    // ─── Prismatic ───────────────────────────────────────────────────────────
    pybind11::module_ prismatic_module = m.def_submodule("prismatic_joint_module", "PrismaticJoint Bindings");
    JOINT_FLOAT(prismatic_module, PrismaticJoint, "axis_angle", GetAxisAngle, SetAxisAngle);
    JOINT_BOOL (prismatic_module, PrismaticJoint, "enable_spring", GetEnableSpring, SetEnableSpring);
    JOINT_FLOAT(prismatic_module, PrismaticJoint, "hertz", GetHertz, SetHertz);
    JOINT_FLOAT(prismatic_module, PrismaticJoint, "damping_ratio", GetDampingRatio, SetDampingRatio);
    JOINT_FLOAT(prismatic_module, PrismaticJoint, "target_translation", GetTargetTranslation, SetTargetTranslation);
    JOINT_BOOL (prismatic_module, PrismaticJoint, "enable_limit", GetEnableLimit, SetEnableLimit);
    JOINT_FLOAT(prismatic_module, PrismaticJoint, "lower_translation", GetLowerTranslation, SetLowerTranslation);
    JOINT_FLOAT(prismatic_module, PrismaticJoint, "upper_translation", GetUpperTranslation, SetUpperTranslation);
    JOINT_BOOL (prismatic_module, PrismaticJoint, "enable_motor", GetEnableMotor, SetEnableMotor);
    JOINT_FLOAT(prismatic_module, PrismaticJoint, "motor_speed", GetMotorSpeed, SetMotorSpeed);
    JOINT_FLOAT(prismatic_module, PrismaticJoint, "max_motor_force", GetMaxMotorForce, SetMaxMotorForce);
    JOINT_FLOAT_RO(prismatic_module, PrismaticJoint, "translation", GetTranslation);

    // ─── Weld ────────────────────────────────────────────────────────────────
    pybind11::module_ weld_module = m.def_submodule("weld_joint_module", "WeldJoint Bindings");
    JOINT_FLOAT(weld_module, WeldJoint, "linear_hertz", GetLinearHertz, SetLinearHertz);
    JOINT_FLOAT(weld_module, WeldJoint, "linear_damping_ratio", GetLinearDampingRatio, SetLinearDampingRatio);
    JOINT_FLOAT(weld_module, WeldJoint, "angular_hertz", GetAngularHertz, SetAngularHertz);
    JOINT_FLOAT(weld_module, WeldJoint, "angular_damping_ratio", GetAngularDampingRatio, SetAngularDampingRatio);

    // ─── Wheel ───────────────────────────────────────────────────────────────
    pybind11::module_ wheel_module = m.def_submodule("wheel_joint_module", "WheelJoint Bindings");
    JOINT_FLOAT(wheel_module, WheelJoint, "axis_angle", GetAxisAngle, SetAxisAngle);
    JOINT_BOOL (wheel_module, WheelJoint, "enable_spring", GetEnableSpring, SetEnableSpring);
    JOINT_FLOAT(wheel_module, WheelJoint, "hertz", GetHertz, SetHertz);
    JOINT_FLOAT(wheel_module, WheelJoint, "damping_ratio", GetDampingRatio, SetDampingRatio);
    JOINT_BOOL (wheel_module, WheelJoint, "enable_limit", GetEnableLimit, SetEnableLimit);
    JOINT_FLOAT(wheel_module, WheelJoint, "lower_translation", GetLowerTranslation, SetLowerTranslation);
    JOINT_FLOAT(wheel_module, WheelJoint, "upper_translation", GetUpperTranslation, SetUpperTranslation);
    JOINT_BOOL (wheel_module, WheelJoint, "enable_motor", GetEnableMotor, SetEnableMotor);
    JOINT_FLOAT(wheel_module, WheelJoint, "motor_speed", GetMotorSpeed, SetMotorSpeed);
    JOINT_FLOAT(wheel_module, WheelJoint, "max_motor_torque", GetMaxMotorTorque, SetMaxMotorTorque);

    // ─── Motor ───────────────────────────────────────────────────────────────
    pybind11::module_ motor_module = m.def_submodule("motor_joint_module", "MotorJoint Bindings");
    motor_module.def("get_linear_velocity", [](const std::string& id) {
        MotorJoint* j = Find<MotorJoint>(id);
        glm::vec2 v = j ? j->GetLinearVelocity() : glm::vec2(0.0f);
        return std::make_tuple(v.x, v.y);
    });
    motor_module.def("set_linear_velocity", [](const std::string& id, float x, float y) {
        if (MotorJoint* j = Find<MotorJoint>(id)) j->SetLinearVelocity({x, y});
    });
    JOINT_FLOAT(motor_module, MotorJoint, "max_velocity_force", GetMaxVelocityForce, SetMaxVelocityForce);
    JOINT_FLOAT(motor_module, MotorJoint, "angular_velocity", GetAngularVelocity, SetAngularVelocity);
    JOINT_FLOAT(motor_module, MotorJoint, "max_velocity_torque", GetMaxVelocityTorque, SetMaxVelocityTorque);
    JOINT_FLOAT(motor_module, MotorJoint, "linear_hertz", GetLinearHertz, SetLinearHertz);
    JOINT_FLOAT(motor_module, MotorJoint, "linear_damping_ratio", GetLinearDampingRatio, SetLinearDampingRatio);
    JOINT_FLOAT(motor_module, MotorJoint, "max_spring_force", GetMaxSpringForce, SetMaxSpringForce);
    JOINT_FLOAT(motor_module, MotorJoint, "angular_hertz", GetAngularHertz, SetAngularHertz);
    JOINT_FLOAT(motor_module, MotorJoint, "angular_damping_ratio", GetAngularDampingRatio, SetAngularDampingRatio);
    JOINT_FLOAT(motor_module, MotorJoint, "max_spring_torque", GetMaxSpringTorque, SetMaxSpringTorque);
}

#undef JOINT_FLOAT
#undef JOINT_BOOL
#undef JOINT_FLOAT_RO
