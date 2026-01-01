#include "engine/Engine.hpp"
#include "engine/core/TimeManager.hpp"
#include <iostream>
#include "engine/debug/Console.hpp"
#include "engine/rendering/Renderer.hpp"
#include "engine/rendering/RenderManager.hpp"
#include "engine/components/ComponentRegistrars.hpp"


void Engine::Init() {
    RegisterComponentTypes();
    InputManager::Get().Init();
    TimeManager::Get().Init(); 
    SceneManager::Get().Init();
}


void Engine::Run() {
    int count = 0;
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
