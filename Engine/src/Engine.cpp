#include "Engine.hpp"
#include "engine/core/TimeManager.hpp"
#include <iostream>
#include "engine/debug/Console.hpp"
#include "engine/rendering/RenderManager.hpp"
#include "engine/components/ComponentRegistrars.hpp"
#include <pybind11/embed.h>
#include "engine/bindings/PythonBindings.hpp"

namespace py = pybind11;

void Engine::Init() {
    static auto* guard = new py::scoped_interpreter();
    static auto* release = new py::gil_scoped_release();
    engine::RegisterPythonBindings();
    RegisterComponentTypes();
    InputManager::Get().Init(); 
    TimeManager::Get().Init(); 
    SceneManager::Get().Init();
}


void Engine::Run() {
    InputManager::Get().Update();
    TimeManager::Get().Update();
    SceneManager::Get().Update();
    RenderManager::Get().Update();
}

void Engine::Shutdown() {
    InputManager::Get().Shutdown();
    SceneManager::Get().Shutdown();
    TimeManager::Get().Shutdown();

}

bool Engine::GetActive(){
    return active;
}

void Engine::SetActive(bool active){
    this->active = active;
}
