#include "engine/core/Application.hpp"

void Application::Init() {
    InputManager::Get().Init();
    SceneManager::Get().Init();
    
}

void Application::Run() {
    while (GetActive()) {
        InputManager::Get().Update();
        SceneManager::Get().Update();
        SetActive(false); 
    }
}

void Application::Shutdown() {
    InputManager::Get().Shutdown();
    SceneManager::Get().Shutdown();
}

bool Application::GetActive(){
    return active;
}

void Application::SetActive(bool active){
    this->active = active;
}
