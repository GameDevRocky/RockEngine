#pragma once
#include <pybind11/pybind11.h>
// SYSTEMS
#include "Engine.hpp"

using namespace EngineUtils;


extern Proxy<PhysicsSystem> physicsSystem;
extern Proxy<Registry> registry;
extern Proxy<InputManager> inputManager;
extern Proxy<TimeManager> timeManager;
namespace engine {
    void RegisterPythonBindings(); 
}