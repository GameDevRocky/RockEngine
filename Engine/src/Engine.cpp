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
#include "engine/input/GamepadService.hpp"
#include "engine/input/CursorManager.hpp"
#include "engine/components/AudioListener.hpp"
#include "engine/components/Camera.hpp"
#include "engine/components/Transform.hpp"
#include <filesystem>
#include <cstdlib>

namespace py = pybind11;

namespace {
void SetPythonHome(const std::string& home) {
#if defined(_WIN32)
    _putenv_s("PYTHONHOME", home.c_str());
#else
    setenv("PYTHONHOME", home.c_str(), 1);
#endif
}

}

void Engine::SetAppMode(AppMode mode) {
   
    if (initialized) {
        std::cerr << "Engine::SetAppMode called after Init(); ignored." << std::endl;
        return;
    }
    appMode = mode;
}

void Engine::Init() {
    MainThread::Stamp();
    JobSystem::Get();
    {
        const std::string bundledHome = EngineUtils::ExecutableDir() + "/python";
        std::error_code ec;
        if (std::filesystem::exists(bundledHome, ec)) {
            SetPythonHome(bundledHome);
        }
#if defined(ROCKENGINE_PYTHON_HOME)
        else if (std::filesystem::exists(ROCKENGINE_PYTHON_HOME, ec)) {
            SetPythonHome(ROCKENGINE_PYTHON_HOME);
        }
#endif
    }

    static auto* guard = new py::scoped_interpreter();
    {
        py::module_ sys = py::module_::import("sys");
        py::list path = sys.attr("path");
        for (const char* rel : { "", "Domain/sandbox/scripts", "Domain/lib" }) {
            path.attr("insert")(0, EngineUtils::GetAssetPath(rel));
        }
#if defined(ROCKENGINE_PYTHON_SITES)
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

    editorContainer = new Container();
    editorContainer->AddSystem(new Registry());
    editorContainer->AddSystem(new TimeManager());
    editorContainer->AddSystem(new InputManager());
    editorContainer->AddSystem(new PhysicsSystem());
    editorContainer->AddSystem(new SceneManager());
    editorContainer->AddSystem(new SelectionManager());
    editorContainer->AddSystem(new LayerManager());
    editorContainer->AddSystem(new TagManager());
    if (IsEditor()) {
        editorContainer->AddSystem(new UndoSystem());
        editorContainer->AddSystem(new FileWatcherSystem());
    }
    editorContainer->SetMode(Container::Mode::Editor);
    editorContainer->Init();
    editorContainer->PostInit();
    activeContainer = editorContainer;
    initialized = true;
    std::cout << "Engine Initialized\n" << std::endl;
}

void Engine::PostInit(){
    AudioEngine::Get().EnsureInitialized();
    GamepadService::Get().EnsureInitialized();
}

void Engine::Update(){
    ROCK_PROFILE_FRAME();
    ROCK_PROFILE_SCOPE("Engine::Update");
    JobSystem::Get().Pump();
    GamepadService::Get().Update();
    activeContainer->Update();
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
    desc.graceMs = 0;
    desc.warmupPumps = 2;
    desc.mainStep = [](JobProgressSink& sink) -> bool {
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
    if (auto* selection = runtimeContainer->FindSystem<SelectionManager>())
        selection->ClearSelection();

    runtimeContainer->Shutdown();
    delete runtimeContainer;
    runtimeContainer = nullptr;
    activeContainer = editorContainer;
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
    JobSystem::Get().Shutdown();
    AudioEngine::Get().Shutdown();
    GamepadService::Get().Shutdown();
}
