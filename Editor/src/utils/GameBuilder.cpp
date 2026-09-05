#include "utils/GameBuilder.hpp"

#include "engine/jobs/JobSystem.hpp"
#include "engine/utils/EngineUtils.hpp"

#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace {

#if defined(_WIN32)
    constexpr const char* kPlayerExeName = "RockEnginePlayer.exe";
    constexpr const char* kExeSuffix     = ".exe";
#else
    constexpr const char* kPlayerExeName = "RockEnginePlayer";
    constexpr const char* kExeSuffix     = "";
#endif

// Strips characters a filesystem will not accept in a name, so "My Game: Reloaded" becomes
// a folder rather than an I/O error. Spaces are kept -- they are legal and games have them.
std::string SanitizeName(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        const bool illegal = (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
                              c == '"' || c == '<'  || c == '>' || c == '|');
        out += illegal ? '_' : c;
    }
    // A trailing dot or space makes a directory Windows cannot open by name.
    while (!out.empty() && (out.back() == '.' || out.back() == ' ')) out.pop_back();
    if (out.empty()) out = "Game";
    return out;
}

// Which RockEnginePlayer.exe a shipped game gets.
//
// This is not a preference, it is a correctness issue, and it is the whole reason the
// windows-msvc-release preset exists. What the player imports depends on how it was built:
//
//   Debug     MSVCP140D.dll, VCRUNTIME140D.dll, ucrtbased.dll -- the DEBUG CRT, which
//             Microsoft ships only with Visual Studio and forbids redistributing. A game
//             built from a Debug player runs on developer machines and nowhere else, which
//             looks exactly like "you need a compiler installed to play my game".
//   Release   MSVCP140.dll / VCRUNTIME140.dll, redistributable but still needing to be
//             copied or installed.
//   Release + ROCKENGINE_STATIC_MSVC_RUNTIME   nothing. The CRT is inside the exe.
//
// So: prefer the export-template tree, which is the last of those. The player sitting next
// to the editor is kept as a fallback -- a repo with no release build should still produce
// something the developer can run -- but Prepare flags it and the Build window warns.
struct PlayerLocation {
    fs::path path;
    bool     isExportTemplate = false;  // came from build/release/bin, i.e. shippable
};

PlayerLocation FindPlayer(const fs::path& editorBinDir)
{
    std::error_code ec;

    // First candidate walks up from the editor's own bin dir, so it works whatever the
    // editor's build directory is called: build/<preset>/bin -> build/release/bin. The
    // second uses the compiled-in source root, for an editor configured somewhere that is
    // not a sibling of the release tree.
    const fs::path candidates[] = {
        editorBinDir.parent_path().parent_path() / "release" / "bin" / kPlayerExeName,
        fs::path(PROJECT_ROOT) / "build" / "release" / "bin" / kPlayerExeName,
    };
    for (const fs::path& candidate : candidates) {
        if (fs::exists(candidate, ec)) return { candidate, true };
    }

    return { editorBinDir / kPlayerExeName, false };
}

// copy_directory with the overwrite behaviour we want and an error we can report. The
// standard's recursive copy stops at the first problem, which is what we want -- a build
// that half-copied its assets should fail loudly, not ship.
bool CopyTree(const fs::path& from, const fs::path& to, std::string& error) {
    std::error_code ec;
    fs::copy(from, to,
             fs::copy_options::recursive | fs::copy_options::overwrite_existing,
             ec);
    if (ec) {
        error = "copying " + from.string() + " -> " + to.string() + ": " + ec.message();
        return false;
    }
    return true;
}

} // namespace

std::string GameBuilder::OutputDirFor(const std::string& gameName, const std::string& parentDir)
{
    if (parentDir.empty()) return {};
    return (fs::path(parentDir) / SanitizeName(gameName)).make_preferred().string();
}

bool GameBuilder::Prepare(const BuildConfig& cfg, const std::string& parentDir,
                          Inputs& out, std::string& error)
{
    std::error_code ec;

    if (parentDir.empty()) {
        error = "No output folder chosen.";
        return false;
    }
    if (cfg.startupScene.empty()) {
        error = "No startup scene selected. The game would have nothing to load.";
        return false;
    }

    // binDir is the editor's own output directory, which is where the staged Python runtime
    // lives. The PLAYER is looked up separately -- see FindPlayer -- because the shippable
    // one comes from the export-template tree, not from beside the editor.
    const fs::path binDir = fs::path(EngineUtils::ExecutableDir());

    const PlayerLocation player = FindPlayer(binDir);
    if (!fs::exists(player.path, ec)) {
        error = std::string("Player binary not found at ") + player.path.string() +
                "\n\nBuild the shippable player first:\n"
                "  cmake --preset windows-msvc-release\n"
                "  cmake --build --preset windows-msvc-release\n\n"
                "Or, for a developer-only build, the player from this tree:\n"
                "  cmake --build --preset local --target RockEnginePlayer";
        return false;
    }

    const fs::path domainDir = fs::path(EngineUtils::GetAssetPath("Domain"));
    if (!fs::exists(domainDir, ec)) {
        error = "Domain/ not found at " + domainDir.string();
        return false;
    }

    // Verify the startup scene actually exists before producing a folder whose executable
    // will fail on launch. Cheap check, and it catches a scene renamed since the config
    // was last saved.
    const fs::path scenePath = fs::path(EngineUtils::GetAssetPath(cfg.startupScene));
    if (!fs::exists(scenePath, ec)) {
        error = "Startup scene not found: " + scenePath.string();
        return false;
    }

    out.cfg           = cfg;
    out.playerExe     = player.path.string();
    out.playerIsExportTemplate = player.isExportTemplate;
    out.domainDir     = domainDir.string();
    out.outputDir     = OutputDirFor(cfg.gameName, parentDir);

    // Staged once by the Player target's build (see Player/CMakeLists.txt). Optional: a
    // build without it still runs on a machine that has Python installed, which is fine for
    // handing to yourself and useless for handing to anyone else -- so the caller warns
    // rather than failing.
    const fs::path runtimeDir = binDir / "player-runtime";
    const fs::path runtime    = runtimeDir / "python";
    out.pythonRuntime = fs::exists(runtime, ec) ? runtime.string() : std::string();

    // pythonXY.dll comes from player-runtime, NOT from binDir. It must not live beside the
    // editor: CPython derives its prefix from wherever its DLL loaded, so a copy in bin/
    // makes the EDITOR look for bin/Lib, find no site-packages, and lose `watchdog` -- which
    // kills Python hot-reload with nothing but a stderr line to show for it.
    out.pythonDllDir = fs::exists(runtimeDir, ec) ? runtimeDir.string() : std::string();

    return true;
}

void GameBuilder::Submit(Inputs inputs, std::function<void(const Result&)> onDone)
{
    // Heap-allocated so the worker and onFinished halves share one result. Freed in
    // onFinished, which is guaranteed to run exactly once for every submitted job.
    auto* result = new Result();
    result->outputDir = inputs.outputDir;

    JobDesc desc;
    desc.title = "Building " + inputs.cfg.gameName;
    // Not modal: LoadingOverlay is a child widget of MainWindow and would not cover the
    // separate top-level Build window anyway. The Build window drives its own progress bar
    // from JobSystem::SnapshotActive(), and the editor stays usable meanwhile.
    desc.modal = false;

    // Everything below runs on the worker thread. It touches ONLY std::filesystem and the
    // by-value captured Inputs -- no Console, no Notify, no Engine::Get(), no Qt. See the
    // prohibitions in Job.hpp; violating them here would corrupt engine state in a way that
    // reproduces roughly never and crashes at random.
    desc.worker = [inputs, result](JobProgressSink& sink) {
        std::error_code ec;
        const fs::path outDir(inputs.outputDir);

        sink.Report(0.05f, "Preparing output folder");

        // A rebuild into an existing folder must not leave last build's stale assets behind
        // -- a scene deleted from the project would otherwise keep shipping forever. Remove
        // only the pieces we own, so pointing the output at a folder holding other files
        // (screenshots, a readme) does not nuke them.
        for (const char* owned : { "Domain", "python" }) {
            fs::remove_all(outDir / owned, ec);
            if (ec) {
                sink.Fail(std::string("clearing ") + owned + ": " + ec.message());
                result->error = std::string("clearing ") + owned + ": " + ec.message();
                return;
            }
        }
        fs::create_directories(outDir, ec);
        if (ec) {
            sink.Fail("creating " + outDir.string() + ": " + ec.message());
            result->error = "creating " + outDir.string() + ": " + ec.message();
            return;
        }

        // --- the executable, renamed to the game -------------------------------------
        sink.Report(0.15f, "Copying the player");
        const fs::path exeDest = outDir / (SanitizeName(inputs.cfg.gameName) + kExeSuffix);
        fs::copy_file(inputs.playerExe, exeDest, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            sink.Fail("copying the player executable: " + ec.message());
            result->error = "copying the player executable: " + ec.message();
            return;
        }
        result->exePath = exeDest.string();

        // --- game content ----------------------------------------------------------------
        // The whole Domain/ tree, which is what makes this work with zero changes to asset
        // loading: EngineUtils::GetAssetRoot() switches to the executable's folder the
        // moment a sibling Domain/ exists, so every baked meta path resolves unchanged.
        sink.Report(0.30f, "Copying game content");
        std::string err;
        if (!CopyTree(inputs.domainDir, outDir / "Domain", err)) {
            sink.Fail(err);
            result->error = err;
            return;
        }

        // __pycache__ is build output from the dev machine, keyed to that machine's exact
        // interpreter. Shipping it is dead weight at best and a stale-bytecode import at
        // worst.
        sink.Report(0.65f, "Pruning build artifacts");
        for (auto it = fs::recursive_directory_iterator(outDir / "Domain", ec);
             !ec && it != fs::recursive_directory_iterator(); )
        {
            const fs::path p = it->path();
            const bool isPycache = it->is_directory(ec) && p.filename() == "__pycache__";
            // Advance BEFORE removing: deleting the entry an iterator points at
            // invalidates it, and recursive_directory_iterator gives no warning.
            it.increment(ec);
            if (isPycache) fs::remove_all(p, ec);
        }
        ec.clear();

        // --- the Python runtime ------------------------------------------------------------
        // Engine::Init probes for "<exeDir>/python" and points PYTHONHOME at it, which is
        // what lets the game run on a machine with no Python installed.
        if (!inputs.pythonRuntime.empty()) {
            sink.Report(0.75f, "Copying the Python runtime");
            if (!CopyTree(inputs.pythonRuntime, outDir / "python", err)) {
                sink.Fail(err);
                result->error = err;
                return;
            }
        }

        // pythonXY.dll has to sit beside the SHIPPED executable, not inside python/ -- it is
        // resolved by the OS loader, not by PYTHONHOME. Missing it is the classic packaging
        // failure: the build looks complete and the game dies instantly on a clean machine.
        //
        // In the build output this placement is correct precisely because a sibling
        // python/Lib exists there, so CPython's prefix resolution finds a real standard
        // library. That is the same mechanism that makes putting these next to the EDITOR
        // wrong -- there is no bin/Lib, so the prefix resolves to nothing.
        if (!inputs.pythonDllDir.empty()) {
            sink.Report(0.90f, "Copying runtime libraries");
            for (const auto& entry : fs::directory_iterator(inputs.pythonDllDir, ec)) {
                if (ec) break;
                const std::string name = entry.path().filename().string();
                const bool isPythonDll = name.rfind("python", 0) == 0 &&
                                         entry.path().extension() == ".dll";
                if (isPythonDll)
                    fs::copy_file(entry.path(), outDir / name,
                                  fs::copy_options::overwrite_existing, ec);
            }
            ec.clear();
        }

        sink.Report(1.0f, "Finishing up");
        result->ok = true;
    };

    // Main thread. BuildConfig::Save is plain yaml-cpp + ofstream and could technically have
    // gone in the worker, but writing the manifest last and here means a build that failed
    // midway never leaves a game.rock claiming success.
    desc.mainStep = [inputs, result](JobProgressSink& sink) -> bool {
        if (!result->ok) return false;
        const std::string manifest = (fs::path(inputs.outputDir) / "game.rock").string();
        if (!BuildConfig::Save(manifest, inputs.cfg)) {
            result->ok = false;
            result->error = "Failed to write " + manifest;
            sink.Fail(result->error);
        }
        return false;
    };

    desc.onFinished = [result, onDone](bool ok, const std::string& error) {
        if (!ok && result->error.empty())
            result->error = error.empty() ? "Build failed." : error;
        result->ok = ok && result->ok;
        if (onDone) onDone(*result);
        delete result;
    };

    JobSystem::Get().Submit(std::move(desc));
}
