
#pragma once
#include "engine/core/System.hpp"
#include "engine/core/GameObject.hpp"

class Registry;

class Inspector : public System {

public:
    Inspector() = default;

    void OnEnterPlayMode() override {}
    void OnExitPlayMode() override {}
    void SetGameObject(GameObject *gameobject){};
    GameObject* GetGameObject(){return nullptr;};


private:
    std::string selected_id = ""; 
    Registry* registry = nullptr;
    ~Inspector() = default;

};