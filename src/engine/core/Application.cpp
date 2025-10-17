#include "engine/core/Application.hpp"
#include "engine/core/TimeManager.hpp"
#include <iostream>

void Application::Init() {
    InputManager::Get().Init();
    SceneManager::Get().Init();
    TimeManager::Get().Init();
    
}

void Application::Run() {
    int count = 0;
    InputManager::Get().Update();
    SceneManager::Get().Update();
    TimeManager::Get().Update();
    SetActive(false);
    
}

void Application::Shutdown() {
    InputManager::Get().Shutdown();
    SceneManager::Get().Shutdown();
    TimeManager::Get().Shutdown();
}

bool Application::GetActive(){
    return active;
}

void Application::SetActive(bool active){
    this->active = active;
}
