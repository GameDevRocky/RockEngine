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

    editorContainer = new Container();
    activeContainer = editorContainer;
    
    editorContainer->Init();
    editorContainer->PostInit();
}


void Engine::Update(){
    activeContainer->Update();   
}


void Engine::EnterPlayMode(){
    runtimeContainer = editorContainer->Copy();
    activeContainer = runtimeContainer;
    activeContainer->Init();
    activeContainer->PostInit();
}

void Engine::ExitPlayMode(){
    runtimeContainer->Shutdown();
    activeContainer = editorContainer;
}

void Engine::Shutdown() {
   
}
