#include "engine/bindings/PythonBindings.hpp"
#include <pybind11/embed.h>
#include <pybind11/stl.h> 
#include "engine/components/Transform.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include <string>

Proxy<PhysicsSystem> physicsSystem;
Proxy<Registry> registry;
Proxy<InputManager> inputManager;
Proxy<TimeManager> timeManager;
Proxy<TagManager> tagManager;
Proxy<SceneManager> sceneManager;

namespace py = pybind11;

// SYSTEMS
void BindInputManager(py::module_& m);
void BindGamepad(py::module_& m);
void BindConsoleManager(py::module_& m);
void BindPhysics(py::module_& m);
void BindDebugDraw(py::module_& m);
void BindScene(py::module_& m);

// CORE
void BindGameObject(py::module_& m);


// COMPONENTS
void BindComponent(py::module_& m);
void BindTransform(py::module_& m);
void BindSpriteRenderer(py::module_& m);
void BindTextRenderer(py::module_& m);
void BindRigidBody(py::module_& m);
void BindCollider(py::module_& m);
void BindCamera(py::module_& m);
void BindAnimator(py::module_& m);
void BindParticleComponent(py::module_& m);
void BindJoint(py::module_& m);
void BindAudioSource(py::module_& m);

// RENDERING
void BindSprite(py::module_ & m);
void BindMaterial(py::module_ & m);
void BindTexture2D(py::module_ & m);

// AUDIO
void BindAudioClip(py::module_& m);

// TIME
void BindTime(py::module_& m);


PYBIND11_EMBEDDED_MODULE(rock_engine, m) {
    m.doc() = "C++ Core Logic for Python Handles"; 
    
    // CORE
    py::module_ core = m.def_submodule("core", " Core RockEngine APIs");
    BindGameObject(core);
    
    // SYSTEMS
    py::module_ systems = m.def_submodule("systems", " RockEngine systems APIs");
    BindInputManager(systems);
    BindGamepad(systems);
    BindConsoleManager(systems);
    BindPhysics(systems);
    BindDebugDraw(systems);
    BindTime(systems);
    BindScene(systems);

    // COMPONENTS
    py::module_ components = m.def_submodule("components", " RockEngine components APIs");
    BindComponent(components);
    BindTransform(components);
    BindSpriteRenderer(components);
    BindTextRenderer(components);
    BindRigidBody(components);
    BindCollider(components);
    BindCamera(components);
    BindAnimator(components);
    BindParticleComponent(components);
    BindJoint(components);
    BindAudioSource(components);

    // RENDERING
    py::module_ rendering = m.def_submodule("rendering", "ReockEngine rendering APIs");
    BindSprite(rendering);
    BindMaterial(rendering);
    BindTexture2D(rendering);

    // AUDIO
    py::module_ audio = m.def_submodule("audio", "RockEngine audio APIs");
    BindAudioClip(audio);

}

namespace engine {
    void RegisterPythonBindings() {
  
    }
}