#include "engine/Engine.hpp"
#include "engine/core/TimeManager.hpp"
#include <iostream>
#include "engine/debug/Console.hpp"
void Engine::Init() {
    InputManager::Get().Init();
    SceneManager::Get().Init();
    TimeManager::Get().Init();  
}

void Engine::Run() {
    int count = 0;
    InputManager::Get().Update();
    SceneManager::Get().Update();
    TimeManager::Get().Update();
    SetActive(false);
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
