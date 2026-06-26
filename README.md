# RockEngine

A 2D game engine with a Qt6 editor and an embedded-Python scripting layer.
C++20 core, OpenGL 4.6 rendering, Box2D physics, YAML serialization, pybind11 scripting.

Builds and runs on **Windows, macOS, and Linux**.

---

## Quick start

```sh
git clone --recursive https://github.com/GameDevRocky/RockEngine.git
cd RockEngine

# Windows (PowerShell):
./setup.ps1

# macOS / Linux:
./setup.sh
```

That's it. The setup script initializes submodules, finds Qt, configures, builds, and launches
the editor. For Qt it uses, in order: an existing install you point it at (`-QtDir` /
`--qt-dir`), a Qt previously downloaded into `./.qt`, a Qt already installed on your machine
(e.g. `C:\Qt\<ver>\msvc2022_64`, `~/Qt/<ver>/<arch>`), and only as a last resort downloads one
via [aqtinstall](https://github.com/miurahr/aqtinstall) (a few hundred MB, once).

> Already cloned without `--recursive`? Run `git submodule update --init --recursive` first
> (the setup scripts also do this for you).

> **If the automatic Qt download fails** (aqtinstall can lag behind Qt's newest releases),
> install Qt yourself with the [official Qt Online Installer](https://www.qt.io/download-qt-installer)
> — choose the **Desktop MSVC 2022 64-bit** (Windows) / **Desktop** (macOS/Linux) component for the
> version in the setup script — then re-run setup. It auto-detects the install. Or pass it
> explicitly: `./setup.ps1 -QtDir "C:/Qt/<ver>/msvc2022_64"` (`./setup.sh --qt-dir …`).

---

## Prerequisites

Standard C++ tooling — the setup scripts handle everything else (Qt, submodules, build):

| Tool | Version | Windows | macOS | Linux |
|------|---------|---------|-------|-------|
| Git | any | [git-scm](https://git-scm.com/download/win) | `xcode-select --install` | `apt install git` |
| CMake | ≥ 3.23 | with Visual Studio, or [cmake.org](https://cmake.org/download/) | `brew install cmake` | `apt install cmake` |
| C++20 compiler | — | Visual Studio 2022/2026 with **Desktop development with C++** | Xcode / Apple Clang | `apt install build-essential` |
| Ninja | any | bundled with Visual Studio | `brew install ninja` | `apt install ninja-build` |
| Python | ≥ 3.10 (with dev headers) | [python.org](https://www.python.org/downloads/) | `brew install python` | `apt install python3 python3-dev python3-venv` |

Python is needed at build time (the engine embeds a Python interpreter via pybind11) and to run
`aqtinstall` for the Qt download. The shipped app bundles its own Python runtime, so end users of
a packaged build do not need Python installed.

---

## Using an IDE

After running setup once, open the folder directly — both IDEs read `CMakePresets.json` plus the
`CMakeUserPresets.json` that setup generated (which points at the downloaded Qt), so Qt is found
automatically with no manual configuration:

- **Visual Studio** (Windows): *File → Open → Folder…* → select the `local` configuration → Build / Debug.
- **VS Code** (any OS): install the **CMake Tools** extension → pick the `local` preset → Build / Debug
  (debug configs are in `.vscode/launch.json`).

---

## Manual build (without the setup script)

If you prefer to manage Qt yourself:

1. Install Qt 6 (Core, Gui, Widgets, OpenGLWidgets).
2. Copy `CMakeUserPresets.json.example` → `CMakeUserPresets.json` and set `CMAKE_PREFIX_PATH`
   to your Qt install (the folder containing `bin/`, `lib/cmake/`, …). On macOS/Linux change
   `inherits` to `macos` / `linux`.
3. Configure, build, run:
   ```sh
   cmake --preset local
   cmake --build --preset local
   # the launcher is in build/local/bin/
   ```
   Or, after setup/manual config, use the convenience wrapper: `./build.sh` (add `--all` to clean first).

Without a `CMakeUserPresets.json` you can instead set the `CMAKE_PREFIX_PATH` environment
variable to your Qt install and use the per-OS presets directly:
`cmake --preset windows-msvc` / `macos` / `linux`.

---

## Project layout

| Path | What it is |
|------|------------|
| `Engine/`   | Runtime core (`RockEngineCore`). No Qt. |
| `Editor/`   | Qt6 editor UI (`RockEngineEditor`). Depends on Engine. |
| `Domain/`   | Game-side content: Python scripting API + assets + sandbox. |
| `src/`      | `main.cpp` — the `RockEngineLauncher` executable. |
| `External/` | Git submodules (glm, yaml-cpp, box2d, pybind11, imgui, imguizmo) + vendored glad/stb. |

See `CLAUDE.md` and the per-directory `CLAUDE.md` files for architecture details.
