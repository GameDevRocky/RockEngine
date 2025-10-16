#pragma once
#include <iostream>
#include <vector>
#include "engine/core/InputManager.hpp"
#include "engine/core/SceneManager.hpp"

class Application {
public:
    static Application& Get() {
        static Application instance;
        return instance;
    }

    void Init();
    void Run();
    void Shutdown();

    void SetActive(bool active);
    bool GetActive();

    
    private:
        bool active = true;
        Application() = default;

        ~Application() = default;

};
