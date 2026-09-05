#include "PlayerApp.hpp"
#include "PlayerInput.hpp"

#include "Engine.hpp"
#include "engine/core/InputManager.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/debug/Console.hpp"
#include "engine/input/GamepadService.hpp"
#include "engine/rendering/Renderer.hpp"
#include "engine/rendering/views/GameRenderView.hpp"
#include "engine/utils/EngineUtils.hpp"

#include <glad/glad.h>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace {
    InputManager* Input() {
        Container* c = Engine::Get()->GetActiveContainer();
        return c ? c->FindSystem<InputManager>() : nullptr;
    }
    SceneManager* Scenes() {
        Container* c = Engine::Get()->GetActiveContainer();
        return c ? c->FindSystem<SceneManager>() : nullptr;
    }
}

bool PlayerApp::Init()
{
    // --- 1. Config -------------------------------------------------------------------
    // game.rock sits beside the executable and is written by the editor's Build window.
    // Running straight out of build/local/bin (no game.rock, no sibling Domain/) is a
    // supported dev path: GetAssetRoot() falls back to PROJECT_ROOT, so the player runs
    // against the live source tree with default window settings.
    const std::string cfgPath = EngineUtils::ExecutableDir() + "/game.rock";
    if (!BuildConfig::Load(cfgPath, cfg))
        std::cout << "[player] no game.rock beside the executable; using defaults "
                     "(dev run against PROJECT_ROOT)" << std::endl;

    if (!sceneOverride.empty()) {
        cfg.startupScene = sceneOverride;
        std::cout << "[player] startup scene overridden from the command line: "
                  << cfg.startupScene << std::endl;
    }

    // --- 2. Engine, before the window ---------------------------------------------------
    // SetAppMode MUST precede Init(): Init() reads it to decide which systems the container
    // gets (no UndoSystem, no FileWatcherSystem in a shipped game).
    Engine::Get()->SetAppMode(AppMode::Player);
    Engine::Get()->Init();

    // --- 3. Window + GL context ----------------------------------------------------------
    // GamepadService::EnsureInitialized (called from Engine::PostInit below) also calls
    // SDL_Init and CoInitializeEx. SDL_Init is refcounted so the overlap is harmless, and
    // claiming the subsystems here first means a video failure is reported before the audio
    // device is opened.
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        std::cerr << "[player] SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // Mirrors the QSurfaceFormat the editor sets in Editor::Init -- same GL version, same
    // profile, same buffer sizes, same MSAA. Any divergence here shows up as a shipped game
    // that renders subtly differently from the Game tab you tested in.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (cfg.resizable)  flags |= SDL_WINDOW_RESIZABLE;
    if (cfg.fullscreen) flags |= SDL_WINDOW_FULLSCREEN;

    window = SDL_CreateWindow(cfg.gameName.c_str(), cfg.windowWidth, cfg.windowHeight, flags);
    if (!window) {
        std::cerr << "[player] SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return false;
    }

    glCtx = SDL_GL_CreateContext(window);
    if (!glCtx) {
        std::cerr << "[player] SDL_GL_CreateContext failed: " << SDL_GetError()
                  << "\n(this build requires an OpenGL 4.6 core profile)" << std::endl;
        return false;
    }
    SDL_GL_MakeCurrent(window, glCtx);

    // Vsync IS the frame pacing -- SDL_GL_SwapWindow blocks on it, which is what replaces
    // the editor's frameSwapped-driven loop. With it off the loop spins as fast as the GPU
    // allows, so honour the config but say so.
    SDL_GL_SetSwapInterval(cfg.vsync ? 1 : 0);

    SDL_GetWindowSizeInPixels(window, &pixelW, &pixelH);

    // --- 4. Renderer + the game view -------------------------------------------------------
    // Must come after the context is current: EnsureInitialized calls gladLoadGL and then
    // does the whole AssetManager bootstrap against it.
    if (!Renderer::Get().EnsureInitialized()) {
        std::cerr << "[player] Renderer::EnsureInitialized failed"
                  << " | GL_VENDOR="   << reinterpret_cast<const char*>(glGetString(GL_VENDOR))
                  << " | GL_RENDERER=" << reinterpret_cast<const char*>(glGetString(GL_RENDERER))
                  << " | GL_VERSION="  << reinterpret_cast<const char*>(glGetString(GL_VERSION))
                  << std::endl;
        return false;
    }
    view = Renderer::Get().CreateGameView(pixelW, pixelH);

    // Left in DisplayMode::FromCamera (the default) on purpose: the camera's authored
    // targetAspect then letterboxes exactly the way the editor's Game tab does, so what
    // you framed in the editor is what ships.

    // --- 5. Audio + gamepads ------------------------------------------------------------------
    Engine::Get()->PostInit();

    // A polled pad has no focus concept of its own. In the editor Qt's applicationStateChanged
    // supplies this; here it comes from SDL window focus events (see PumpEvents). Seed it true
    // -- a window that just opened has focus, and without the seed every pad reads as zeroes
    // until the user happens to alt-tab away and back.
    GamepadService::Get().SetApplicationFocused(true);

    // --- 6. Startup scene ------------------------------------------------------------------------
    if (cfg.startupScene.empty()) {
        std::cerr << "[player] no startupScene set in game.rock -- nothing to load." << std::endl;
        return false;
    }

    // LoadScene is ASYNCHRONOUS: the YAML parse runs on a JobSystem worker and the object
    // graph is built from the job's main step a frame or two later. Subscribing to
    // LOADED_SCENE_EVENT is the only correct way to know it landed -- the scene does not
    // exist when this call returns. Returning true keeps the subscription alive, but this
    // one is genuinely one-shot, so it returns false to auto-unsubscribe (the Observable
    // idiom) and a second scene load later cannot re-enter play mode.
    if (SceneManager* scenes = Scenes()) {
        scenes->Subscribe([this]() {
            sceneLoaded = true;
            return false;
        }, SceneManager::LOADED_SCENE_EVENT);
        scenes->LoadScene(cfg.startupScene);
    } else {
        std::cerr << "[player] no SceneManager on the active container." << std::endl;
        return false;
    }

    running = true;
    return true;
}

void PlayerApp::BeginPlay()
{
    // The synchronous EnterPlayMode, not RequestEnterPlayMode: the deferred variant exists so
    // the editor's loading overlay gets a chance to paint, and there is no overlay here. This
    // deep-copies the editor container into a Runtime one exactly as the Play button does, so
    // the world that ships is produced by the same code path you tested against.
    Engine::Get()->EnterPlayMode();
    playing = true;
}

void PlayerApp::OnResized()
{
    SDL_GetWindowSizeInPixels(window, &pixelW, &pixelH);
    if (pixelW < 1) pixelW = 1;
    if (pixelH < 1) pixelH = 1;
}

void PlayerApp::PumpEvents()
{
    InputManager* input = Input();

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                running = false;
                break;

            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_RESIZED:
                OnResized();
                break;

            // Without this a game left running in the background keeps responding to a
            // controller while you work in another app. GamepadService zeroes every read
            // while unfocused but keeps devices open, so nothing re-enumerates on the way
            // back. Same behaviour the editor gets from Qt's applicationStateChanged.
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                GamepadService::Get().SetApplicationFocused(true);
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                GamepadService::Get().SetApplicationFocused(false);
                // Keyboard state is pushed, not polled, so a key held at the moment focus is
                // lost never receives its release event and would stay down forever. The
                // editor has the same shape of problem and dodges it by only receiving input
                // while a viewport has focus; here we have to clear explicitly.
                if (input) {
                    for (int qtKey : { 0x01000020, 0x01000021, 0x01000023 })  // shift/ctrl/alt
                        input->SetKeyState(qtKey, false);
                }
                break;

            case SDL_EVENT_KEY_DOWN:
                // e.key.repeat filters auto-repeat: InputManager derives its just-pressed
                // edge from a state transition, and a repeat is not one. Letting repeats
                // through would still be harmless for IsKeyDown but would make IsKeyPressed
                // fire over and over while a key is held.
                if (input && !e.key.repeat) {
                    const int qtKey = PlayerInput::ToQtKey(e.key.key);
                    if (qtKey >= 0) input->SetKeyState(qtKey, true);
                }
                break;

            case SDL_EVENT_KEY_UP:
                if (input) {
                    const int qtKey = PlayerInput::ToQtKey(e.key.key);
                    if (qtKey >= 0) input->SetKeyState(qtKey, false);
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (input) {
                    const int qtButton = PlayerInput::ToQtMouseButton(e.button.button);
                    if (qtButton >= 0)
                        input->SetMouseButtonState(qtButton, e.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if (input && view) {
                    // InputManager stores mouse position in SCREEN space and converts to world
                    // on read, so the value stays correct when the camera moves without the
                    // mouse. It expects DPI-scaled framebuffer pixels, and SDL reports window
                    // coordinates, so scale first (the two differ on a high-DPI display,
                    // exactly like Qt's devicePixelRatioF).
                    int winW = 0, winH = 0;
                    SDL_GetWindowSize(window, &winW, &winH);
                    const float sx = winW > 0 ? static_cast<float>(pixelW) / winW : 1.0f;
                    const float sy = winH > 0 ? static_cast<float>(pixelH) / winH : 1.0f;
                    const glm::vec2 fbPos(e.motion.x * sx, e.motion.y * sy);
                    input->SetMouseScreenPosition(view, fbPos);
                }
                break;

            default:
                break;
        }
    }
}

int PlayerApp::Run()
{
    while (running) {
        PumpEvents();

        // Engine::Update pumps the JobSystem, and the startup scene load finishes from
        // inside that pump -- so this has to be ticked before the scene exists, not after.
        Engine::Get()->Update();

        if (sceneLoaded && !playing)
            BeginPlay();

        // A minimized window has a zero-size framebuffer; rendering into it is wasted at
        // best and a GL error at worst. Keep calling Update() though, so scripts, coroutines
        // and audio carry on rather than freezing until the window is restored.
        const bool minimized = (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) != 0;
        if (!minimized && pixelW > 0 && pixelH > 0) {
            view->Resize(pixelW, pixelH);
            view->Render();
            // 0 is the default framebuffer. This is the one line that differs from
            // ViewportWidget::paintGL, which passes Qt's defaultFramebufferObject().
            view->Present(0, pixelW, pixelH);

            // Checked after Render(), not before: HasActiveCamera reflects the camera
            // resolved during the last UpdateCamera(), so it is meaningless until a frame
            // has actually gone through. Warn once -- the editor draws an on-screen notice
            // for this, and a shipped game showing black with no explanation anywhere is a
            // miserable thing to debug.
            if (playing && !warnedNoCamera && !view->HasActiveCamera()) {
                warnedNoCamera = true;
                Console::Warn("No enabled Camera component in the startup scene -- "
                              "the game is rendering an empty view.");
            }
        }

        // Blocks on vsync when the swap interval is 1. This IS the frame pacing -- there is
        // no watchdog timer here because there is nothing to rescue: the loop cannot stall
        // waiting on a repaint that never comes, the way the editor's can.
        SDL_GL_SwapWindow(window);
    }
    return 0;
}

void PlayerApp::Shutdown()
{
    // Renderer::DestroyView needs the context current, so it has to happen before the
    // context is torn down.
    if (view && glCtx) {
        SDL_GL_MakeCurrent(window, glCtx);
        Renderer::Get().DestroyView(view);
        view = nullptr;
    }

    // Joins the job worker, closes the audio device, and zeroes every rumble motor -- the
    // motors latch in the controller's own hardware and would otherwise outlive the process.
    Engine::Get()->Shutdown();

    if (glCtx)  { SDL_GL_DestroyContext(glCtx); glCtx = nullptr; }
    if (window) { SDL_DestroyWindow(window);    window = nullptr; }
    SDL_Quit();
}
