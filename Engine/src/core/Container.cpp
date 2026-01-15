#include "engine/core/Container.hpp"


Container::Container(Mode mode){
    SetMode(mode);
}


Container* Container::Copy(){
    Container* container = new Container(Mode::Runtime);
    for (auto* system : systems){
        auto* copy = system->Copy();
        container->AddSystem(copy);
    }
    return container;
}



void Container::Init(){
    if (initialized) return;

    for (System* system : systems) {
        system->Init();
    }

    initialized = true;
}

void Container::PostInit(){
    for (System* system : systems) {
        system->PostInit();
    }
    
}

void Container::Update(){

    for (System* system : systems) {
        system->Update();
    }

}

void Container::Shutdown(){
    for (System* system : systems) {
        system->Shutdown();
    }
}