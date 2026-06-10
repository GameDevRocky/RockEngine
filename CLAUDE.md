# RockEngine

A 2D game engine with a Qt6 editor and an embedded-Python scripting layer. C++20 core,
OpenGL 4.6 rendering, Box2D physics, YAML-based serialization, pybind11 scripting.

## Top-level layout

| Path        | What it is | Details |
|-------------|------------|---------|
| `Engine/`   | Runtime core (`RockEngineCore` lib). No Qt. | `Engine/CLAUDE.md` |
| `Editor/`   | Qt6 editor UI (`RockEngineEditor` lib). Depends on Engine. | `Editor/CLAUDE.md` |
| `Domain/`   | Game-side content: Python scripting API + assets + sandbox. Not compiled. | `Domain/CLAUDE.md` |
| `src/`      | `main.cpp` — the launcher executable `RockEngineLauncher`. | — |
| `External/` + `external/` | Git submodules: glm, yaml-cpp, box2d, glad, pybind11, imgui, imguizmo. | `.gitmodules` |
| `tools/`    | Misc tooling. | — |

Each major layer has its own `CLAUDE.md` (loaded when you work in that subtree) with the
detail. This root file is cross-cutting only.

> Casing matters in git: `Domain/` and `External/` (capital) vs `external/` (lowercase).
> Windows is case-insensitive but git tracks both — match existing paths.

## Build & run

Two build setups; pick the one matching the user's environment.

**MinGW (build.sh — scripted: configure + build + run):**
```bash
./build.sh          # configure (if needed) + build + run
./build.sh --all    # nuke build/qt-mingw-debug first, then full rebuild
```
Builds into `build/qt-mingw-debug/`, runs `bin/RockEngineLauncher.exe`. Qt 6.11.0 mingw_64 +
bundled mingw1310 toolchain.

**MSVC / Visual Studio (CMake presets):**
```bash
cmake --preset Qt-Debug-VS      # VS 2026 generator, msvc2022_64 Qt, out/build/debug
cmake --build build --config Debug
```
The allowlisted incremental build command in this repo is `cmake --build build --config Debug`.

Requirements: CMake ≥ 3.20, C++20, Qt6 (Core Gui Widgets OpenGLWidgets), Python (embedded
interpreter — see `requirements.txt`). `PROJECT_ROOT` is compiled in as a define; asset paths
resolve relative to it.

## How the layers fit together

- `src/main.cpp` runs `Engine::Init → Editor::Init → Engine::PostInit → Editor::PostInit`
  (the last blocks in the Qt event loop). The editor's `QTimer` (~16ms) drives
  `Engine::Get()->Update()` every frame.
- **Engine** is the runtime world (containers of systems + game objects). **Editor** observes
  and drives the engine via its event system; it holds no game logic. **Domain** is data and
  Python scripts loaded at runtime. Engine exposes C++ to Python; Domain's handlers wrap it.
- Two big invariants to know before changing anything:
  - **Play mode deep-copies the editor world**, runs it, and discards it — so anything that
    must survive play mode has to be copyable via `Copy()`. (Details in `Engine/CLAUDE.md`.)
  - **Observable/Event** is the decoupling backbone across all layers — a notify callback that
    returns `false` auto-unsubscribes. (Details in `Engine/CLAUDE.md`.)

## Repo-wide conventions

- C++20. MSVC: `/Zc:__cplusplus`, `/EHsc`. GCC/Clang: `-Wall -Wextra`.
- Headers in `*/include/`, sources in `*/src/`, mirrored structure. Engine headers use the
  `engine/...` include prefix.
- Singletons expose a static `Get()` (`Engine::Get()`, `AssetManager::Get()`,
  `MainWindow::Get()`).
- Don't edit submodule directories (`External/`, `external/`) — they're upstream.
- `__pycache__/*.pyc` show up in git status; they're build artifacts, ignore them.
