#include "Engine.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/debug/FrameProfiler.hpp"
#include <iostream>
#include "engine/debug/Console.hpp"
#include "engine/components/ComponentRegistrars.hpp"
#include <pybind11/embed.h>
#include "engine/bindings/PythonBindings.hpp"
#include "engine/core/PhysicsSystem.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/core/UndoSystem.hpp"
#include "engine/core/LayerManager.hpp"
#include "engine/core/TagManager.hpp"
#include "engine/core/FileWatcherSystem.hpp"
#include "engine/jobs/JobSystem.hpp"
#include "engine/jobs/MainThread.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/audio/AudioEngine.hpp"
#include "engine/components/AudioListener.hpp"
#include "engine/components/Camera.hpp"
#include "engine/components/Transform.hpp"
#include <filesystem>
#include <cstdlib>

namespace py = pybind11;

void Engine::Init() {
    // Claim this thread as the engine's before anything else runs, so
    // ROCK_ASSERT_MAIN_THREAD has something to compare against and every job
    // worker is measured from here.
    MainThread::Stamp();

    // Construct the job system eagerly rather than on first Submit: Get() is a
    // magic static, which is thread-safe to initialize but only if the first
    // touch isn't racing a worker that doesn't exist yet. Doing it here makes
    // the ordering unambiguous.
    JobSystem::Get();

    // If a bundled Python runtime sits next to the executable (distribution build),
    // point the embedded interpreter at it so the app runs without a system Python.
    // In a dev build there is no such folder, so the build-time interpreter is used
    // and this is a no-op (behavior unchanged).
    {
        const std::string bundledHome = EngineUtils::ExecutableDir() + "/python";
        std::error_code ec;
        if (std::filesystem::exists(bundledHome, ec)) {
#if defined(_WIN32)
            _putenv_s("PYTHONHOME", bundledHome.c_str());
#else
            setenv("PYTHONHOME", bundledHome.c_str(), 1);
#endif
        }
    }

    static auto* guard = new py::scoped_interpreter();

    // Make the engine's Python API importable without relying on a PYTHONPATH env var:
    // add the asset root (so "import Domain.lib.api..." resolves) plus the script and
    // lib folders. GetAssetPath resolves against the app root (bundled or PROJECT_ROOT).
    {
        py::module_ sys = py::module_::import("sys");
        py::list path = sys.attr("path");
        for (const char* rel : { "", "Domain/sandbox/scripts", "Domain/lib" }) {
            path.attr("insert")(0, EngineUtils::GetAssetPath(rel));
        }
    }

    static auto* release = new py::gil_scoped_release();

    engine::RegisterPythonBindings();
    RegisterComponentTypes();

    // No GL context needed (unlike Renderer), so this can start eagerly here rather than
    // lazily on first use.
    AudioEngine::Get().EnsureInitialized();

    editorContainer = new Container();
    editorContainer->AddSystem(new Registry());
    editorContainer->AddSystem(new TimeManager());
    editorContainer->AddSystem(new InputManager());
    editorContainer->AddSystem(new PhysicsSystem());
    editorContainer->AddSystem(new SceneManager());
    editorContainer->AddSystem(new SelectionManager());
    // After SelectionManager: UndoSystem::Init subscribes to it to break the
    // coalescing gesture when the selection changes.
    editorContainer->AddSystem(new UndoSystem());
    editorContainer->AddSystem(new LayerManager());
    editorContainer->AddSystem(new TagManager());
    editorContainer->AddSystem(new FileWatcherSystem());

    editorContainer->SetMode(Container::Mode::Editor);
    editorContainer->Init();
    editorContainer->PostInit();
    activeContainer = editorContainer;
    std::cout << "Engine Initialized\n" << std::endl;
}

void Engine::PostInit(){
    
}

void Engine::Update(){
    ROCK_PROFILE_FRAME();
    ROCK_PROFILE_SCOPE("Engine::Update");

    // Before the container ticks, deliberately: a job that swaps containers
    // (entering play mode) must complete before anything reads the container
    // this frame. Note there is NO GL context current here -- Editor calls
    // Update() outside paintGL, and its stall watchdog calls it when no viewport
    // is presenting at all -- so jobs park results and the draw path uploads.
    JobSystem::Get().Pump();

    activeContainer->Update();

    // Pulled once per frame, same "pull, don't push" rule the render cameras follow (see
    // Engine/CLAUDE.md "Rendering"): whichever AudioListener is active drives what every
    // AudioSource's spatialBlend measures distance/pan against, falling back to the main
    // Camera so positional audio works with zero setup in scenes that never add one.
    AudioEngine::Get().Update();
    if (AudioListener* listener = AudioListener::GetMain()) {
        if (Transform* t = listener->GetTransform())
            AudioEngine::Get().SetListenerPosition(glm::vec3(t->GetWorldPosition(), 0.0f));
    } else if (Camera* cam = Camera::GetMain()) {
        if (Transform* t = cam->GetTransform())
            AudioEngine::Get().SetListenerPosition(glm::vec3(t->GetWorldPosition(), 0.0f));
    }
}


void Engine::RequestEnterPlayMode(){
    if (playModeTransitionInFlight) return;
    if (!editorContainer || activeContainer != editorContainer) return;
    playModeTransitionInFlight = true;

    JobDesc desc;
    desc.title = "Entering play mode";
    desc.modal = true;
    // No grace delay and a warmup: this operation is KNOWN to block for its
    // whole duration, so the card must be on screen and painted BEFORE the work
    // starts, not after a delay that it would outlive. Costs ~2 frames.
    desc.graceMs = 0;
    desc.warmupPumps = 2;

    // No worker half, and that is not an oversight -- see the header.
    desc.mainStep = [](JobProgressSink& sink) -> bool {
        // Indeterminate: the copy is one indivisible call, so there is no
        // honest fraction to report. A fake advancing bar would be a lie.
        sink.Report(-1.0f, "Copying world");
        Engine::Get()->EnterPlayMode();
        return false;
    };
    desc.onFinished = [](bool, const std::string&) {
        Engine::Get()->playModeTransitionInFlight = false;
    };

    JobSystem::Get().Submit(std::move(desc));
}

void Engine::RequestExitPlayMode(){
    if (playModeTransitionInFlight) return;
    if (!runtimeContainer || activeContainer != runtimeContainer) return;
    playModeTransitionInFlight = true;

    JobDesc desc;
    desc.title = "Exiting play mode";
    desc.modal = true;

    desc.mainStep = [](JobProgressSink& sink) -> bool {
        sink.Report(-1.0f, "Tearing down runtime world");
        Engine::Get()->ExitPlayMode();
        return false;
    };
    desc.onFinished = [](bool, const std::string&) {
        Engine::Get()->playModeTransitionInFlight = false;
    };

    JobSystem::Get().Submit(std::move(desc));
}

void Engine::EnterPlayMode(){
    runtimeContainer = editorContainer->Copy();
    
    runtimeContainer->SetMode(Container::Mode::Runtime);
    runtimeContainer->Init();
    runtimeContainer->PostInit();
    
    activeContainer = runtimeContainer;
    runtimeContainer->Awake();
    runtimeContainer->Start();
    Notify(ENTER_PLAY_MODE_EVENT);
}

void Engine::ExitPlayMode(){
    // Clear the runtime selection BEFORE tearing the container down. Otherwise
    // Registry::Shutdown deletes objects one by one, and each dying object's
    // SHUTDOWN_EVENT drives SelectionManager::RemoveFromSelection ->
    // SELECTION_CHANGED -> the editor Inspector rebuild, which walks its stored
    // Observable* handles — some already freed earlier in that same delete loop
    // (use-after-free). Clearing here fires that one SELECTION_CHANGED while every
    // object is still alive (so the Inspector unsubscribes cleanly) and drops the
    // per-object shutdown-subs, so the teardown loop raises no selection cascade.
    if (auto* selection = runtimeContainer->FindSystem<SelectionManager>())
        selection->ClearSelection();

    runtimeContainer->Shutdown();
    delete runtimeContainer;
    runtimeContainer = nullptr;
    activeContainer = editorContainer;
    Notify(EXIT_PLAY_MODE_EVENT);
}

void Engine::PauseMode(){
    if (activeContainer->GetMode() != Container::Mode::Runtime) return;
    activeContainer->SetPaused(true);
    Notify(PAUSE_MODE_EVENT);
}

void Engine::ResumeMode(){
    if (activeContainer->GetMode() != Container::Mode::Runtime) return;
    activeContainer->SetPaused(false);
    Notify(RESUME_MODE_EVENT);
}

void Engine::StepFrame(){
    if (activeContainer->GetMode() != Container::Mode::Runtime) return;
    activeContainer->StepFrame();
}

void Engine::Shutdown() {
    // Join the worker before anything it might publish into goes away. The
    // editor calls this first in its own Shutdown (while Qt is still alive);
    // this call is the net for a headless embedding, and is idempotent.
    JobSystem::Get().Shutdown();
    AudioEngine::Get().Shutdown();
}
