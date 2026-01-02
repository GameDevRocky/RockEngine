#include "engine/inspection/inspector.hpp"

GameObject* Inspector::GetGameObject(){
    return gameobject;
}

void Inspector::SetGameObject(GameObject* gameobject){
    this->gameobject = gameobject;
}