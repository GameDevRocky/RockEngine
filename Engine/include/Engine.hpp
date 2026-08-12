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
    static inline const Event PAUSE_MODE_EVENT = Engine::CreateEvent();
    static inline const Event RESUME_MODE_EVENT = Engine::CreateEvent();
    
    static Engine* Get() {
        static Engine* instance = new Engine();
        return instance;
    }
    
    
    void Init();
    void PostInit();
    void Update();
    void Shutdown();

    // Deferred play-mode transitions. These submit a job and return
    // immediately; the transition itself runs from the job's main step on a
    // later frame, so the loading overlay gets a chance to paint before an
    // operation that blocks for its whole duration.
    //
    // Nothing here is threaded and nothing can be: Registry::Copy runs a
    // Serializable constructor (and therefore GenerateUUID) per object, Box2D
    // bodies are created, and every ScriptComponent re-imports its Python module
    // under the GIL. The job buys feedback, not parallelism.
    //
    // Prefer these over the synchronous pair below -- they carry the re-entrancy
    // guard that stops a double-click from copying the world twice.
    void RequestEnterPlayMode();
    void RequestExitPlayMode();
    bool IsTransitioningPlayMode() const { return playModeTransitionInFlight; }

    // The synchronous transitions. Still public for scripted/headless callers
    // that genuinely want to block; the editor goes through Request* instead.
    void EnterPlayMode();
    void ExitPlayMode();

    void PauseMode();
    void ResumeMode();
    void StepFrame();
    bool IsPaused() const { return activeContainer && activeContainer->IsPaused(); }

    Container* GetActiveContainer(){ return activeContainer;}
    Container* GetEditorContainer(){ return editorContainer;}
    Container* GetRuntimeContainer(){ return runtimeContainer;}

    
    
    private:
    Engine() = default;
    ~Engine() = default;

    Container* editorContainer = nullptr;
    Container* runtimeContainer = nullptr;
    Container* activeContainer = nullptr;

    // A transition job is queued or running. The overlay blocks input, but that
    // is UI and UI is not a guarantee -- a script, a shortcut, or a second click
    // landing in the same frame would otherwise submit a second transition and
    // deep-copy the world twice. This flag is the actual invariant.
    bool playModeTransitionInFlight = false;

    
};

namespace EngineUtils {
    template <typename T>
    class Proxy {
    public:
        T* operator->() const {
            return Engine::Get()->GetActiveContainer()->FindSystem<T>();
        }
        T& operator*() const {
            return *Engine::Get()->GetActiveContainer()->FindSystem<T>();
        }
        explicit operator bool() const {
            return Engine::Get()->GetActiveContainer()->FindSystem<T>() != nullptr;
        }
    };
}
