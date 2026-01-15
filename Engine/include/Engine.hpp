#pragma once
#include <iostream>
#include <vector>
#include "engine/core/InputManager.hpp"
#include "engine/core/SceneManager.hpp"
#include <memory>
#include "engine/core/Container.hpp"

class Engine {
public:
    static Engine* Get() {
        static Engine* instance = new Engine();
        return instance;
    }
    
    
    void Init();
    void Update();
    void Shutdown();
    void Toggle();

    void EnterPlayMode();
    void ExitPlayMode();

    void CreateContainer();

    Container* GetActiveContainer(){ return activeContainer;}
    Container* GetEditorContainer(){ return editorContainer;}
    Container* GetRuntimeContainer(){ return runtimeContainer;}

    
    
    private:
    Engine() = default;
    ~Engine() = default;

    Container* editorContainer = nullptr;
    Container* runtimeContainer = nullptr;
    Container* activeContainer = nullptr;

    
};
