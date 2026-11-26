#include "PlayerApp.hpp"

#include <SDL3/SDL.h>
// SDL_main.h has to be included EXPLICITLY, in the one translation unit that defines main.
// SDL3 split it out of SDL.h (SDL2 pulled it in for you), and it is what renames main to
// SDL_main and emits the real platform entry point -- on Windows, a WinMain that parses the
// command line into UTF-8 argv and calls SDL_SetMainReady.
//
// Without it a Release build fails to link with
//     LIBCMT.lib(exe_winmain.obj) : LNK2019: unresolved external symbol WinMain
// because WIN32_EXECUTABLE puts the exe on the /subsystem:windows entry point (see
// Player/CMakeLists.txt) while this file still only provides main(). Debug builds hide it:
// they keep the console subsystem, which does want main().
#include <SDL3/SDL_main.h>

// Entry point for RockEnginePlayer -- the standalone game executable.
//
// The editor's counterpart is src/main.cpp, which runs Engine::Init -> Editor::Init ->
// Engine::PostInit -> Editor::PostInit. This is the same shape with the Qt half removed:
// PlayerApp owns the window and the loop where Editor owned the QApplication.
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
