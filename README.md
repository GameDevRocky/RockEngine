# RockEngine

A 2D Game Engine with a QT editor, python scripting, and C++ 20 runtime.

## Platforms

- Windows: supported
- Linux: TBA
- macOS: TBA

## Requirements

- Visual Studio 2022 or newer with **Desktop development with C++**
- CMake 3.25+
- Python 3.10+ with development headers
- Qt 6 (MSVC 64-bit)
- OpenGL 4.6-capable graphics driver
- Ninja (used by the current CMake presets)
- Git

## Setup

From the project folder, run:

```powershell
.\setup.ps1
```

Setup initializes Git submodules, finds or downloads Qt, builds the engine, and opens the editor.

## Tests

With tests enabled in the local CMake configuration, run:

```powershell
.\run-tests.ps1
```

## External libraries

- [Qt](https://www.qt.io/)
- [GLM](https://github.com/g-truc/glm)
- [yaml-cpp](https://github.com/jbeder/yaml-cpp)
- [Box2D](https://github.com/erincatto/box2d)
- [pybind11](https://github.com/pybind/pybind11)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)
- [msdfgen](https://github.com/chlumsky/msdfgen)
- [FreeType](https://github.com/freetype/freetype)
- [SDL3](https://github.com/libsdl-org/SDL)
- [GLAD](https://github.com/Dav1dde/glad)
- [stb](https://github.com/nothings/stb)
- [miniaudio](https://github.com/mackron/miniaudio)
- [Watchdog](https://github.com/gorakhargosh/watchdog)
- [GoogleTest](https://github.com/google/googletest) (tests)
- [NVRHI](https://github.com/NVIDIA-RTX/NVRHI) (tracked submodule; currently unused)
