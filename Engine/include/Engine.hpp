#pragma once
#include <iostream>
#include <vector>
#include "engine/core/InputManager.hpp"
#include "engine/core/SceneManager.hpp"
#include <memory>
#include "engine/core/Container.hpp"
#include "engine/core/Observable.hpp"

class Engine : public Observable {
public:
    static inline const Event ENTER_PLAY_MODE_EVENT = Engine::CreateEvent();
    static inline const Event EXIT_PLAY_MODE_EVENT = Engine::CreateEvent();
    static Engine* Get() {
        static Engine* instance = new Engine();
        return instance;
    }
    
    
    void Init();
    void PostInit();
    void Update();
    void Shutdown();

    void EnterPlayMode();
    void ExitPlayMode();

    void LoadDefaultScene();

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
