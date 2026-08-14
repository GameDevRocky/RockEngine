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
already built and sitting in `build/local/bin/`, so a game build *copies* it plus `Domain/` plus
the staged Python runtime and writes a `game.rock` beside them. Seconds, and the machine needs
no toolchain — RockEngine's scripts are Python, so there is nothing to compile, exactly like
Unity's Mono backend.

The copy runs as a `JobSystem` job (worker half — `std::filesystem` only, and all the
worker-thread prohibitions in `Job.hpp` apply), with progress on the Build window's own progress
bar rather than `LoadingOverlay`, which is a child of `MainWindow` and would not cover a separate
top-level window.

`Engine::AppMode` distinguishes the two processes at runtime and gates the authoring-only
behaviour that would otherwise write into a read-only install. See `Engine/CLAUDE.md`.

## Repo-wide conventions

- C++20. MSVC: `/Zc:__cplusplus`, `/EHsc`. GCC/Clang: `-Wall -Wextra`.
- Headers in `*/include/`, sources in `*/src/`, mirrored structure. Engine headers use the
  `engine/...` include prefix.
- Singletons expose a static `Get()` (`Engine::Get()`, `AssetManager::Get()`,
  `MainWindow::Get()`).
- Don't edit submodule directories (`External/`, `external/`) — they're upstream.
- `__pycache__/*.pyc` show up in git status; they're build artifacts, ignore them.
