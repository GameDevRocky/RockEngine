#pragma once

#include "engine/core/BuildConfig.hpp"

#include <SDL3/SDL.h>
#include <string>

class GameRenderView;

// The standalone game host. This is the Player's answer to Editor:: -- it owns the window,
// the GL context and the frame loop, and nothing else. All game logic lives in Engine
// exactly as it does in the editor; this class only feeds it.
//
// The whole reason a separate host is even possible is that RockEngineCore is Qt-free and
// RenderView's contract is already "hand me an FBO id and a pixel size". Where
// ViewportWidget::paintGL passes defaultFramebufferObject(), this passes 0 -- the default
// framebuffer of an SDL window. Nothing else about rendering differs.
class PlayerApp {
public:
    // Overrides game.rock's startupScene. Must be called before Init(). Exists for the
    // command-line dev run; shipped games never use it.
    void SetSceneOverride(const std::string& scenePath) { sceneOverride = scenePath; }

    // Reads game.rock beside the executable, opens the window, brings the engine up and
    // loads the startup scene. Returns false with a message on stderr if any of that fails.
    bool Init();

    // Blocks until the window closes. Returns the process exit code.
    int Run();

    void Shutdown();

private:
    void PumpEvents();
    void OnResized();

    // Called once the startup scene has finished loading (LoadScene is asynchronous), which
    // is the point at which the world is real enough to start simulating.
    void BeginPlay();

    BuildConfig      cfg;
    std::string      sceneOverride;
    SDL_Window*      window      = nullptr;
    SDL_GLContext    glCtx       = nullptr;
    GameRenderView*  view        = nullptr;

    int  pixelW      = 0;
    int  pixelH      = 0;
    bool running        = false;
    bool playing        = false;   // BeginPlay has run; the runtime container is live
    bool sceneLoaded    = false;
    bool warnedNoCamera = false;   // the "nothing is rendering" notice is once-per-run
};
