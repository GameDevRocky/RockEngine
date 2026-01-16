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

void Container::EnterPlayMode(){
    SetMode(Mode::Runtime);

    for (System* system : systems) {
        system->OnEnterPlayMode();
    }

}

void Container::Update(){
    int count = 0;
    for (System* system : systems) {
        std::cout << system << std::endl;
        system->Update();
        std::cout << count << std::endl;
        count ++;
    }
    

}

void Container::Shutdown(){
    for (System* system : systems) {
        system->Shutdown();
    }
}