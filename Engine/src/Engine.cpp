#include "Engine.hpp"
#include "engine/core/TimeManager.hpp"
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
#include "engine/utils/EngineUtils.hpp"
#include <filesystem>
#include <cstdlib>

namespace py = pybind11;

void Engine::Init() {
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
    activeContainer->Update();
       
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
   
}
