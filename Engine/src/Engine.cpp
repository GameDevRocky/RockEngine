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
#include "engine/core/Globals.hpp"
#include "engine/core/FileWatcherSystem.hpp"
#include "engine/jobs/JobSystem.hpp"
#include "engine/jobs/MainThread.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/audio/AudioEngine.hpp"
#include "engine/input/GamepadService.hpp"
#include "engine/input/CursorManager.hpp"
#include "engine/components/AudioListener.hpp"
#include "engine/components/Camera.hpp"
#include "engine/components/Transform.hpp"
#include <filesystem>
#include <cstdlib>

namespace py = pybind11;

namespace {

// PYTHONHOME has to be in the environment before the interpreter is created: PyConfig reads
// it during initialization, and nothing after that can move the prefix.
void SetPythonHome(const std::string& home) {
#if defined(_WIN32)
    _putenv_s("PYTHONHOME", home.c_str());
#else
    setenv("PYTHONHOME", home.c_str(), 1);
#endif
}

} // namespace

void Engine::SetAppMode(AppMode mode) {
    // Init() reads this to decide which systems the container gets, so a call after Init()
    // would silently leave the mode and the container disagreeing -- the kind of mismatch
    // that only shows up as a missing system three layers down. Refuse loudly instead.
    if (initialized) {
        std::cerr << "Engine::SetAppMode called after Init(); ignored." << std::endl;
        return;
    }
    appMode = mode;
}

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

    // Decide where the embedded interpreter's prefix comes from, before it exists.
    // A bundled runtime next to the executable (distribution build) wins, so a shipped
    // game runs on a machine with no Python at all; a dev build falls back to the
    // interpreter this build was configured against.
    {
        const std::string bundledHome = EngineUtils::ExecutableDir() + "/python";
        std::error_code ec;
        if (std::filesystem::exists(bundledHome, ec)) {
            SetPythonHome(bundledHome);
        }
#if defined(ROCKENGINE_PYTHON_HOME)
        // Dev build: pin the interpreter to the Python this build was configured against.
        // Left unpinned, CPython derives sys.prefix from whichever python3XX.dll the loader
        // resolved, and on Windows any unrelated app that ships one earlier on PATH (the AWS
        // CLI does) wins over the real installation. The stdlib still imports -- the registry
        // supplies it -- but sys.prefix lands on the working directory, so
        // "<prefix>/Lib/site-packages" never reaches sys.path and every pip-installed package
        // quietly disappears, taking FileWatcherSystem's watchdog with it.
        else if (std::filesystem::exists(ROCKENGINE_PYTHON_HOME, ec)) {
            SetPythonHome(ROCKENGINE_PYTHON_HOME);
        }
#endif
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
#if defined(ROCKENGINE_PYTHON_SITES)
        // The site-packages directories of the configure-time interpreter, appended (not
        // inserted) so they never shadow Domain. PYTHONHOME above covers the base
        // installation's own site-packages; these add the ones site.py cannot infer from it
        // -- a virtualenv's, and the per-user one -- so requirements.txt works wherever a
        // developer happened to pip install it.
        const std::string sites = ROCKENGINE_PYTHON_SITES;
        std::error_code siteEc;
        for (size_t begin = 0; begin <= sites.size(); ) {
            const size_t sep = sites.find('|', begin);
            const std::string dir = sites.substr(begin, sep - begin);
            if (!dir.empty() && std::filesystem::exists(dir, siteEc)) {
                path.attr("append")(dir);
            }
            if (sep == std::string::npos) break;
            begin = sep + 1;
        }
#endif
    }

    static auto* release = new py::gil_scoped_release();

    engine::RegisterPythonBindings();
    RegisterComponentTypes();
    // Process-global project settings: initialized once, never attached to or
    // copied with either world container.
    Globals::Get();

    editorContainer = new Container();
    editorContainer->AddSystem(new Registry());
    editorContainer->AddSystem(new TimeManager());
    editorContainer->AddSystem(new InputManager());
    editorContainer->AddSystem(new PhysicsSystem());
    editorContainer->AddSystem(new SceneManager());
    // Kept in both modes even though selection is an editing concept: it is nearly free, and
    // ExitPlayMode + several bindings resolve it. A player that skipped it would exercise a
    // different teardown path than the one the editor tests every time you press Stop.
    editorContainer->AddSystem(new SelectionManager());
    editorContainer->AddSystem(new LayerManager());
    editorContainer->AddSystem(new TagManager());

    // Editor-only systems. A shipped game has no undo history to keep and no source tree to
    // watch -- FileWatcherSystem imports Domain.lib.utils.file_watcher and tails
    // Domain/sandbox/scripts for hot-reload, which is also the only reason requirements.txt
    // contains `watchdog`. Skipping it is what lets the shipped Python bundle be stdlib-only.
    if (IsEditor()) {
        // After SelectionManager: UndoSystem::Init subscribes to it to break the
        // coalescing gesture when the selection changes.
        editorContainer->AddSystem(new UndoSystem());
        editorContainer->AddSystem(new FileWatcherSystem());
    }

    // Always Editor here, in BOTH app modes, and that is deliberate: the player loads its
    // startup scene into this container and then calls EnterPlayMode(), so the world it runs
    // is a deep copy produced by exactly the same path the editor's Play button uses. One
    // code path means a shipped game cannot diverge from what you tested.
    editorContainer->SetMode(Container::Mode::Editor);
    editorContainer->Init();
    editorContainer->PostInit();
    activeContainer = editorContainer;
    initialized = true;
    std::cout << "Engine Initialized\n" << std::endl;
}

void Engine::PostInit(){
    // Deferred out of Init() only because nothing needs audio that early (AudioClip probing
    // uses a standalone ma_decoder with no device; an AudioSource only creates a sound in play
    // mode), so the device isn't opened until the app is otherwise up.
    //
    // What this is NOT is the fix for the COM-apartment / drag-and-drop interaction -- that
    // lives in EnsureInitialized itself and is order-independent on purpose, because the point
    // at which Qt initializes OLE is an internal detail we must not depend on. See the comment
    // there before moving this call anywhere.
    AudioEngine::Get().EnsureInitialized();

    // Same deal as audio: a controller is hardware with no per-world identity, so the service
    // lives outside any Container and is started once here rather than per-Container. Safe to
    // sit after the audio call -- both claim an STA COM apartment defensively, so whichever
    // runs first agrees with Qt and the ordering does not matter (see GamepadService.cpp).
    GamepadService::Get().EnsureInitialized();
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

    // Must precede the container tick: InputManager::Update() derives this frame's gamepad
    // pressed/released edges from this state, and polling after the tick would hand every
    // script last frame's buttons -- a one-frame lag on every controller input.
    GamepadService::Get().Update();

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

    // Rumble is the one thing a script can start that outlives its own container: the motors
    // latch in the controller until told otherwise, so a game stopped mid-shake would leave the
    // pad buzzing on the editor's desk with nothing left running to turn it off.
    GamepadService::Get().StopAllRumble();
    CursorManager::Get().Reset();

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
    // Closes every open pad, and zeroes the motors on the way out -- rumble latches in the
    // controller's own hardware and would otherwise outlive the process.
    GamepadService::Get().Shutdown();
}
