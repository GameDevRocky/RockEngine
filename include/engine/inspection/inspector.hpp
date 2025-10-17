
#pragma once
#include "engine/core/System.hpp"
#include "engine/core/GameObject.hpp"
class Inspector : public System {

public:
static Inspector& Get() {
        static Inspector instance;
        return instance;
    }

void SetGameObject(GameObject *gameobject);
GameObject* GetGameObject();


private:
GameObject *gameobject = nullptr;
Inspector();
~Inspector() = default;

};