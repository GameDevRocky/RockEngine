#include "PlayerApp.hpp"

#include <SDL3/SDL.h>

// Entry point for RockEnginePlayer -- the standalone game executable.
//
// The editor's counterpart is src/main.cpp, which runs Engine::Init -> Editor::Init ->
// Engine::PostInit -> Editor::PostInit. This is the same shape with the Qt half removed:
// PlayerApp owns the window and the loop where Editor owned the QApplication.
//
// SDL_main: SDL3's headers rename main to SDL_main and supply the real platform entry
// point, which is what makes a WIN32_EXECUTABLE (no console window) still get its argv.
// Don't "simplify" this back to a bare main -- on Windows the link fails without it.
int main(int argc, char* argv[])
{
    PlayerApp app;

    // One positional argument overrides the startup scene. A shipped game never passes it
    // -- game.rock supplies the scene -- but it makes `RockEnginePlayer.exe some.scene` a
    // usable dev loop straight out of build/bin, with no packaging step, which is by far
    // the fastest way to check that a change works outside the editor.
    if (argc > 1 && argv[1] && argv[1][0] != '\0')
        app.SetSceneOverride(argv[1]);

    if (!app.Init()) {
        // Init already explained itself on stderr. A shipped game is usually launched with
        // no console attached, so also put it in front of the user -- a window that never
        // appears with no message anywhere is indistinguishable from a crash.
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "RockEngine",
                                 "The game failed to start. See the log for details.",
                                 nullptr);
        app.Shutdown();
        return 1;
    }

    const int code = app.Run();
    app.Shutdown();
    return code;
}
