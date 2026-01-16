#include "Engine.hpp"
#include "engine/core/TimeManager.hpp"
#include <iostream>
#include "engine/debug/Console.hpp"
#include "engine/rendering/RenderManager.hpp"
#include "engine/components/ComponentRegistrars.hpp"
#include <pybind11/embed.h>
#include "engine/bindings/PythonBindings.hpp"

#define SAMPLE_SCENE_PATH "Domain/scenes/SampleScene.yaml"

namespace py = pybind11;

void Engine::Init() {
    static auto* guard = new py::scoped_interpreter();
    static auto* release = new py::gil_scoped_release();
    engine::RegisterPythonBindings();
    RegisterComponentTypes();    
    CreateContainer();
    
}

void Engine::LoadDefaultScene(){
    SceneManager* sceneManager = editorContainer->FindSystem<SceneManager>();
    sceneManager->LoadScene(SAMPLE_SCENE_PATH);


}

void Engine::CreateContainer(){
    editorContainer = new Container();
    editorContainer->AddSystem(new Registry());
    editorContainer->AddSystem(new SceneManager());
    editorContainer->AddSystem(new TimeManager());
    editorContainer->AddSystem(new InputManager());
    activeContainer = editorContainer;
    editorContainer->Init();
    LoadDefaultScene();
    editorContainer->PostInit();
    editorContainer->EnterPlayMode();
}


void Engine::Update(){
    
    activeContainer->Update();   
}


void Engine::EnterPlayMode(){
    runtimeContainer = editorContainer->Copy();
    activeContainer = runtimeContainer;
    
    runtimeContainer->Init();
    runtimeContainer->PostInit();
    runtimeContainer->EnterPlayMode();
    std::cout<< "Post Initialized" << std::endl;
}

void Engine::ExitPlayMode(){
    runtimeContainer->Shutdown();
    activeContainer = editorContainer;
}

void Engine::Shutdown() {
   
}
