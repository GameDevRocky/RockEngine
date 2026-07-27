#pragma once
#include <pybind11/pybind11.h>
// SYSTEMS
#include "Engine.hpp"
#include "engine/core/TagManager.hpp"
#include "engine/core/SceneManager.hpp"

using namespace EngineUtils;


extern Proxy<PhysicsSystem> physicsSystem;
extern Proxy<Registry> registry;
extern Proxy<InputManager> inputManager;
extern Proxy<TimeManager> timeManager;
extern Proxy<TagManager> tagManager;
extern Proxy<SceneManager> sceneManager;
namespace engine {
    void RegisterPythonBindings(); 
}