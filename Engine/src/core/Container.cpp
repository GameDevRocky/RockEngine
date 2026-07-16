#include "engine/core/Container.hpp"


Container::Container(Mode mode){
    SetMode(mode);
}


Container* Container::Copy(){
    Container* container = new Container(Mode::Runtime);
    for (auto* system : systems){
        auto* copy = system->Copy(container);
        container->AddSystem(copy);
    }
    return container;
}

void Container::Init(){    
    for (System* system : systems) {
        system->Init();
    }
}

void Container::PostInit(){
    for (System* system : systems) {
        system->PostInit();
    }   
}

void Container::Awake(){
    for (System* system : systems) {
        system->Awake();
    }   
}

void Container::Start(){
    for (System* system : systems) {
        system->Start();
    }   
}

void Container::Update(){
    for (System* system : systems) {
        system->Update();
    }
}

void Container::StepFrame(){
    if (auto* sceneManager = FindSystem<SceneManager>()) {
        sceneManager->StepFrame();
    }
}

void Container::Shutdown() {
    for (auto it = systems.rbegin(); it != systems.rend(); ++it) {
        if (*it) {
            (*it)->Shutdown(); 
        }
    }
    for (System* system : systems) {
        delete system;
    }
    systems.clear();
    systemIndex.clear();
}