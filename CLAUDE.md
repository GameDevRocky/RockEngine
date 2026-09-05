# RockEngine

A 2D game engine with a Qt6 editor and an embedded-Python scripting layer. C++20 core,
OpenGL 4.6 rendering, Box2D physics, YAML-based serialization, pybind11 scripting.

## Top-level layout

| Path        | What it is | Details |
|-------------|------------|---------|
| `Engine/`   | Runtime core (`RockEngineCore` lib). No Qt. | `Engine/CLAUDE.md` |
| `Editor/`   | Qt6 editor UI (`RockEngineEditor` lib). Depends on Engine. | `Editor/CLAUDE.md` |
| `Player/`   | `RockEnginePlayer` — the standalone game exe. SDL3 window, no Qt. | `Player/CLAUDE.md` |
| `Domain/`   | Game-side content: Python scripting API + assets + sandbox. Not compiled. | `Domain/CLAUDE.md` |
| `src/`      | `main.cpp` — the launcher executable `RockEngineLauncher`. | — |
| `External/` | Git submodules (glm, yaml-cpp, box2d, pybind11, imgui, imguizmo, SDL) + vendored glad/stb. | `.gitmodules` |
| `tools/`    | Misc tooling. | — |

Each major layer has its own `CLAUDE.md` (loaded when you work in that subtree) with the
detail. This root file is cross-cutting only.

> All top-level dirs are capitalized (`Engine/`, `Editor/`, `Domain/`, `External/`) and tracked
> that way in git. glad and stb are **vendored** (plain files under `External/`), not submodules.

## Build & run

Cross-platform (Windows / macOS / Linux). The one-command bootstrap is the normal path:

```sh
./setup.ps1     # Windows (PowerShell)
./setup.sh      # macOS / Linux
```
It inits submodules, downloads Qt via `aqtinstall` into `./.qt` (pinned version in the script),
writes a gitignored `CMakeUserPresets.json` pointing `CMAKE_PREFIX_PATH` at it, then configures
+ builds. Output goes to `build/local/bin/`. See `README.md` for prerequisites and IDE usage.

**CMake presets** (machine-agnostic, in `CMakePresets.json`): `windows-msvc`, `macos`, `linux`
(per-OS `condition`s, Ninja, no hardcoded paths — Qt comes from `CMAKE_PREFIX_PATH`). The
generated `local` preset (in `CMakeUserPresets.json`) inherits the right one and pins the Qt
path. Configure/build directly with `cmake --preset local && cmake --build --preset local`, or
use `./build.sh`. Visual Studio (Open Folder) and VS Code (CMake Tools) read these presets.

Requirements: CMake ≥ 3.23, C++20, Qt6 (Core Gui Widgets OpenGLWidgets), Python ≥ 3.10 with dev
headers (embedded interpreter — see `requirements.txt`). `PROJECT_ROOT` is compiled in;
`EngineUtils::GetAssetPath()` resolves assets against the executable dir for bundled builds, else
`PROJECT_ROOT`. Setting `ROCKENGINE_BUNDLE_PYTHON=ON` bundles a standalone Python runtime +
`Domain/` next to the launcher for distribution (see `CMakeLists.txt`).

## How the layers fit together

- `src/main.cpp` runs `Engine::Init → Editor::Init → Engine::PostInit → Editor::PostInit`
  (the last blocks in the Qt event loop). The frame loop is **vsync-driven**: the Scene view's
  `frameSwapped` signal drives `Engine::Get()->Update()` every frame, so FPS tracks the
  monitor's refresh rate (a ~16ms `QTimer` is only a stall-recovery watchdog). See
  `Editor/CLAUDE.md`.
- **Engine** is the runtime world (containers of systems + game objects). **Editor** observes
  and drives the engine via its event system; it holds no game logic. **Domain** is data and
  Python scripts loaded at runtime. Engine exposes C++ to Python; Domain's handlers wrap it.
- **Player** is the third host: same Engine, SDL3 instead of Qt. It exists because Engine is
  genuinely Qt-free and `RenderView::Present()` takes an FBO id — the editor hands it Qt's
  `defaultFramebufferObject()`, the player hands it `0`. See `Player/CLAUDE.md`.
- Two big invariants to know before changing anything:
  - **Play mode deep-copies the editor world**, runs it, and discards it — so anything that
    must survive play mode has to be copyable via `Copy()`. (Details in `Engine/CLAUDE.md`.)
  - **Observable/Event** is the decoupling backbone across all layers — a notify callback that
    returns `false` auto-unsubscribes. (Details in `Engine/CLAUDE.md`.)

## Shipping a game

**File → Build Game…** (Ctrl+Shift+B) opens `BuildWindow`, which authors a `BuildConfig`
(`Domain/sandbox/project.build`) and hands it to `GameBuilder`. Output defaults to the OS
Downloads folder and is never inside the repo.

This is Unity's **export-template** model, not a compiler invocation: `RockEnginePlayer.exe` is
already built, so a game build *copies* it plus `Domain/` plus the staged Python runtime and
writes a `game.rock` beside them. Seconds, and the machine needs no toolchain — RockEngine's
scripts are Python, so there is nothing to compile, exactly like Unity's Mono backend.

**Build the export template before shipping anything.** `GameBuilder` prefers
`build/release/bin/RockEnginePlayer.exe`, produced by a preset that exists only for this:

```sh
cmake --preset windows-msvc-release      # or macos-release / linux-release
cmake --build --preset windows-msvc-release
```

It builds Engine + Player with no Qt and no editor, in Release, and on Windows with
`ROCKENGINE_STATIC_MSVC_RUNTIME=ON` (`/MT`) so the shipped exe imports **no CRT DLL at all** —
only `python314.dll`, which the build copies beside it. Without it `GameBuilder` falls back to
the player next to the editor and the Build window warns, because that binary links whatever
CRT the editor does: from the usual Debug build, `MSVCP140D.dll` / `VCRUNTIME140D.dll` /
`ucrtbased.dll`, which ship only with Visual Studio and may not be redistributed — a zip built
that way runs on developer machines and nowhere else.

The static CRT is why the release lane cannot build the editor: Qt's binaries use the dynamic
CRT, and the two in one process is heap corruption. The root `CMakeLists.txt` makes that a
configure-time error rather than a runtime mystery.

The copy runs as a `JobSystem` job (worker half — `std::filesystem` only, and all the
worker-thread prohibitions in `Job.hpp` apply), with progress on the Build window's own progress
bar rather than `LoadingOverlay`, which is a child of `MainWindow` and would not cover a separate
top-level window.

`Engine::AppMode` distinguishes the two processes at runtime and gates the authoring-only
behaviour that would otherwise write into a read-only install. See `Engine/CLAUDE.md`.

## Testing & CI

`Tests/` is a headless doctest suite over `RockEngineCore` (`RockEngineTests`). Run it with
the same preset CI uses, which needs **no Qt** and builds into its own directory:

```sh
cmake --preset ci-windows        # or ci-linux
cmake --build --preset ci-windows
ctest --preset ci-windows
```

(VS Code: the **RockEngine: test** task does all three.) It takes seconds — `ROCKENGINE_BUILD_EDITOR=OFF`
skips Qt and moc entirely, and `ROCKENGINE_STAGE_PLAYER_RUNTIME=OFF` skips the CPython
standard-library copy.

**The rule for what goes in `Tests/`: no GPU, no window, no Qt, and no `Engine::PostInit()`**
(which opens an audio device and calls `SDL_Init`). `Engine::Init()` itself is window- and
GL-free, so a second tier covering `Scene` snapshot/restore, the `Container::Copy()` play-mode
invariant, `UndoSystem`/`commands/` and `PhysicsSystem` stepping is possible and not yet
written. Anything needing a GL context belongs in the `verify` skill, not here.

`Tests/engine/SerializationRoundTripTests.cpp` is **data-driven over
`SerializableFactory::GetRegisteredTypeNames()`** — register a new component in
`ComponentRegistrars.cpp` and it is immediately covered for factory creation, YAML round-trip
stability and `Copy()` fidelity (the play-mode deep-copy invariant). Write a dedicated test
only for behaviour beyond that contract.

`.github/workflows/ci.yml` runs the same suite on `ubuntu-latest` with GCC on every push and
PR. It builds Engine + Player + tests but **not** the editor — the editor is exercised
constantly by local Windows builds, whereas GCC catches what MSVC never will (missing
transitive includes, stricter `-Wall -Wextra`). Adding an editor job is a ~20-line follow-up
using `jurplel/install-qt-action`.

## Repo-wide conventions

- C++20. MSVC: `/Zc:__cplusplus`, `/EHsc`. GCC/Clang: `-Wall -Wextra`.
- Headers in `*/include/`, sources in `*/src/`, mirrored structure. Engine headers use the
  `engine/...` include prefix.
- Singletons expose a static `Get()` (`Engine::Get()`, `AssetManager::Get()`,
  `MainWindow::Get()`).
- Don't edit submodule directories (`External/`, `external/`) — they're upstream.
- `__pycache__/*.pyc` show up in git status; they're build artifacts, ignore them.
