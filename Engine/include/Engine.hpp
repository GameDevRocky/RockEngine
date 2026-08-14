#pragma once
#include <iostream>
#include <vector>
#include "engine/core/InputManager.hpp"
#include "engine/core/SceneManager.hpp"
#include <memory>
#include "engine/core/Container.hpp"
#include "engine/core/Observable.hpp"

// Which kind of process the engine is running inside. Deliberately NOT on Container:
// Container::Mode (Editor/Runtime/Paused) describes a WORLD, this describes the PROCESS,
// and the two are orthogonal -- a shipped game's container is still Mode::Runtime, and it
// still deep-copies an editor container to get there. Putting this on Container would also
// mean Container::Copy() carries it, which is meaningless for a value that can never change
// after startup. It belongs with Renderer/AudioEngine/GamepadService: process-global state
// with no per-world identity.
enum class AppMode {
    Editor,   // the Qt editor process (RockEngineLauncher)
    Player    // a shipped standalone game (RockEnginePlayer)
};

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

    // Must be called BEFORE Init(): Init() decides which systems to build from it, so setting
    // it afterwards would leave a container that disagrees with the mode. Defaults to Editor,
    // so every existing caller (and any embedding that never calls this) is unchanged.
    void SetAppMode(AppMode mode);
    AppMode GetAppMode() const { return appMode; }
    bool IsPlayer() const { return appMode == AppMode::Player; }
    bool IsEditor() const { return appMode == AppMode::Editor; }

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

    AppMode appMode = AppMode::Editor;
    bool initialized = false;   // guards SetAppMode against a too-late call

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
