#pragma once
#include <iostream>
#include <vector>
#include "engine/core/InputManager.hpp"
#include "engine/core/SceneManager.hpp"
#include <memory>


class Engine {
public:
    static Engine& Get() {
        static Engine instance;
        return instance;
    }
    
    
    void Init();
    void Init(char *args[]);
    void Run();
    void Shutdown();
    
    void SetActive(bool active);
    bool GetActive();
    
    
    private:
        bool active = true;
        Engine() = default;
        ~Engine() = default;

};
