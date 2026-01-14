#include "engine/core/Container.hpp"


Container::Container(Mode mode){
    SetMode(mode);

    registry = new Registry();
    sceneManager = new SceneManager();
    inputManager = new InputManager();
    timeManager = new TimeManager();

}


Container* Container::Copy(){
    Container* container = new Container(Mode::Runtime);
    return container;
}



void Container::Init(){
    if (initialized) return;

    registry->Attach(this);
    sceneManager->Attach(this);
    timeManager->Attach(this);
    inputManager->Attach(this);

    registry->Init();
    timeManager->Init();
    inputManager->Init();
    sceneManager->Init();

    initialized = true;
}

void Container::PostInit(){
    registry->PostInit();
    sceneManager->PostInit();
    timeManager->PostInit();
    inputManager->PostInit();
    
}

void Container::Update(){
    
    registry->Update();
    sceneManager->Update();
    timeManager->Update();
    inputManager->Update();

}

void Container::Shutdown(){
    registry->Shutdown();
    sceneManager->Shutdown();
    timeManager->Shutdown();
    inputManager->Shutdown();
}