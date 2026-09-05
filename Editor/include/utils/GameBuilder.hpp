#pragma once

#include "engine/core/BuildConfig.hpp"

#include <functional>
#include <string>

// Produces a shippable game folder from a BuildConfig.
//
// This is Unity's export-template model, not a compiler invocation: RockEnginePlayer.exe is
// already built (it sits next to the editor in build/bin), so "building a game" is copying
// that binary, the Domain/ content and the staged Python runtime into an output folder and
// writing a game.rock beside them. Seconds, and no toolchain on the machine.
//
// Runs as a JobSystem job so the editor stays responsive. The copying happens on the worker
// half, which is legal because std::filesystem touches no engine state -- but see the hard
// prohibitions in Job.hpp: no Console, no Notify, no Engine::Get(), no Qt on that thread.
// Everything user-facing happens in the completion callback.
class GameBuilder {
public:
    struct Result {
        bool        ok = false;
        std::string error;
        std::string outputDir;   // the folder that was produced
        std::string exePath;     // the game executable inside it
    };

    // Everything the worker needs, resolved on the main thread before it starts, because
    // half of it (the asset root, the executable dir) comes from engine calls the worker is
    // not allowed to make.
    struct Inputs {
        BuildConfig cfg;
        std::string outputDir;      // <chosen folder>/<GameName>
        std::string playerExe;      // source RockEnginePlayer.exe
        // False means playerExe came from the editor's own build tree rather than from the
        // export-template one (build/release/bin), so it links whatever CRT the editor does
        // -- for a Debug editor that is the debug CRT, which only machines with Visual
        // Studio installed have. Diagnostic only; the Build window warns on it. See
        // FindPlayer in GameBuilder.cpp.
        bool        playerIsExportTemplate = false;
        std::string domainDir;      // source Domain/
        std::string pythonRuntime;  // source bin/player-runtime/python (may be empty)
        // bin/player-runtime, which holds pythonXY.dll. Deliberately NOT the editor's own
        // directory: a pythonXY.dll sitting beside the editor breaks the editor's own
        // interpreter (wrong prefix -> no site-packages -> no watchdog -> no hot reload).
        // See the comment block in Player/CMakeLists.txt.
        std::string pythonDllDir;
    };

    // Resolves Inputs from a config, and reports why if it cannot. Main thread only.
    static bool Prepare(const BuildConfig& cfg, const std::string& parentDir,
                        Inputs& out, std::string& error);

    // Where a build with this name lands under `parentDir`. Public so the Build window can
    // show the user the exact path BEFORE they press Build, using the same name-sanitizing
    // the builder itself applies -- two implementations would eventually disagree and the
    // preview would start lying.
    static std::string OutputDirFor(const std::string& gameName, const std::string& parentDir);

    // Submits the build job. `onDone` runs on the main thread when it finishes.
    static void Submit(Inputs inputs, std::function<void(const Result&)> onDone);
};
