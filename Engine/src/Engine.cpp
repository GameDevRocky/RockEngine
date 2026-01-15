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
    CreateContainer();
}

void Engine::CreateContainer(){
    editorContainer = new Container();
    editorContainer->AddSystem(new Registry());
    editorContainer->AddSystem(new SceneManager());
    editorContainer->AddSystem(new TimeManager());
    editorContainer->AddSystem(new InputManager());
    activeContainer = editorContainer;
    editorContainer->Init();
    editorContainer->PostInit();
}



void Engine::Toggle(){
    if (activeContainer == editorContainer) EnterPlayMode();
    else if (activeContainer == runtimeContainer) activeContainer = editorContainer;
}


void Engine::Update(){
    activeContainer->Update();   
}


void Engine::EnterPlayMode(){
    runtimeContainer = editorContainer->Copy();
    activeContainer = runtimeContainer;
    std::cout<< "Post Initialized" << std::endl;
}

void Engine::ExitPlayMode(){
    runtimeContainer->Shutdown();
    activeContainer = editorContainer;
}

void Engine::Shutdown() {
   
}
