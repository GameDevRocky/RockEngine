# Player (`RockEnginePlayer`)

The standalone game executable — what a person who is not you actually runs. Links
`RockEngineCore` and **nothing from `Editor/`**; no Qt anywhere. SDL3 owns the window and the
GL context where the editor's `QOpenGLWidget` owns them.

This is Unity's **export-template** model. The player is compiled once and "File → Build Game…"
then *copies* it. Nothing about making a game build invokes a compiler, because RockEngine's
scripts are Python — interpreted, never compiled — which is the same reason Unity's Mono
backend needs no toolchain at build time.

It is built **twice**, and the difference matters:

| Where | Built by | What it is for |
|---|---|---|
| `build/local/bin/` | the normal `local` preset, alongside the editor | running the player yourself, straight out of the build tree |
| `build/release/bin/` | `cmake --preset windows-msvc-release` | **the binary that ships.** Release, and on Windows `/MT` |

`GameBuilder` prefers the second and warns when it falls back to the first, because the
local one links the editor's CRT — for a Debug build that is `MSVCP140D.dll` /
`VCRUNTIME140D.dll` / `ucrtbased.dll`, which exist only where Visual Studio is installed and
cannot legally be redistributed. The release template imports no CRT DLL at all. See the
`ROCKENGINE_STATIC_MSVC_RUNTIME` block in the root `CMakeLists.txt`.

Two consequences of `WIN32_EXECUTABLE` being on outside Debug, both easy to trip over:
the shipped player has **no console**, so `Console::*` output goes nowhere a user can see;
and `main.cpp` must include `<SDL3/SDL_main.h>` for SDL to supply `WinMain`. Drop that
include and Debug still builds while Release fails to link.

## Why this is even possible

Two properties of the engine, both of which predate this target and neither of which may be
broken:

- **`RockEngineCore` has no Qt in it.** Not "mostly" — zero Qt symbols.
- **`RenderView`'s host contract is an FBO id.** `Present(unsigned targetFBO, int w, int h)`.
  `ViewportWidget::paintGL` passes Qt's `defaultFramebufferObject()`; `PlayerApp` passes `0`,
  the SDL window's default framebuffer. That one argument is the entire rendering difference
  between the editor and a shipped game.

## Layout

- `src/main.cpp` — entry point. Sets `AppMode::Player`, optionally takes a scene path as
  `argv[1]`.
- `src/PlayerApp.cpp` — window, GL context, startup sequence, frame loop. The counterpart to
  `Editor::Init` + `Editor::FrameTick`.
- `src/PlayerInput.cpp` — SDL → Qt input-code translation. See below; this is the part that
  looks pointless and is not.

## Startup sequence

```
SetAppMode(AppMode::Player)   <- BEFORE Init(); Init() builds its system list from it
Engine::Init()
SDL_Init + window + GL 4.6 core context + SDL_GL_SetSwapInterval(1)
Renderer::EnsureInitialized()      <- gladLoadGL + AssetManager bootstrap; needs a current context
Renderer::CreateGameView(w, h)
Engine::PostInit()                 <- audio device + GamepadService
sceneManager->LoadScene(cfg.startupScene)   <- ASYNC
   ...pump Engine::Update() until LOADED_SCENE_EVENT...
Engine::EnterPlayMode()            <- deep-copy -> Mode::Runtime, Awake, Start
```

**`LoadScene` is asynchronous** — the YAML parse runs on a JobSystem worker and the object
graph is built from the job's main step a frame or two later. The scene does not exist when
the call returns; `LOADED_SCENE_EVENT` is the only correct signal. `Engine::Update()` has to
be ticked for that job to finish, so the wait is a real loop, not a block.

**The player enters play mode rather than starting a Runtime container directly.** That is
deliberate: `Engine::Init` always builds an `Editor`-mode container, the startup scene loads
into it, and `EnterPlayMode()` deep-copies it exactly as the editor's Play button does. One
code path means a shipped game cannot diverge from what you tested. It costs one throwaway
copy of the world at startup, which is the right trade.

## Frame loop

```cpp
PumpEvents();
Engine::Get()->Update();           // JobSystem::Pump -> GamepadService -> container -> audio
view->Resize(pixelW, pixelH);
view->Render();
view->Present(0, pixelW, pixelH);
SDL_GL_SwapWindow(window);         // vsync blocks HERE -- this is the frame pacing
```

`SDL_GL_SwapWindow` under `SetSwapInterval(1)` *is* the pacing, so there is no watchdog timer
here. The editor needs one (`Editor`'s ~16ms `QTimer`) because its loop is driven by a
repaint signal that can stop arriving; this loop cannot stall that way.

A minimized window skips render/present but **keeps calling `Engine::Update()`**, so scripts,
coroutines, and audio carry on instead of freezing.

The view is left in `DisplayMode::FromCamera` so the camera's authored `targetAspect`
letterboxes exactly as the editor's Game tab does.

## Input: the Qt keycode translation

**`InputManager::SetKeyState` takes `Qt::Key` values, and those values are baked into the
public scripting API.** `Domain/lib/api/systems/input_system.py` hardcodes
`Keys.ESCAPE = 0x01000000`, `Keys.LEFT = 0x01000012`, `MouseButton.LEFT = 1`. Every game
script ever written against this engine holds Qt numbers.

SDL numbers the same keys completely differently, and the easiest divergence to miss is that
**SDL reports letters as lowercase ASCII (`'a'` = 0x61) while Qt uses uppercase
(`Qt::Key_A` = 0x41)**. Feed SDL's codes through unchanged and digits keep working while
every letter, arrow, modifier, function key and mouse button silently stops — a bug that
appears *only* in shipped builds.

So `PlayerInput` translates and the scripting API is left alone. An unmapped key is dropped,
not passed through. Introducing a real engine-side keycode enum is the better long-term
answer, but it breaks every existing user script and is a separate change.

Three more things `PlayerApp` must replicate from `GameViewGui`, all easy to forget:

- **Mouse position is stored in WORLD space**, not screen space — push it through
  `view->ScreenToWorld()` or `Input.get_mouse_pos()` returns garbage.
- **Focus gating.** `GamepadService::SetApplicationFocused` comes from Qt's
  `applicationStateChanged` in the editor and from `SDL_EVENT_WINDOW_FOCUS_GAINED/LOST` here.
  Without it every pad reads as zeroes forever.
- **Key release on focus loss.** Keyboard state is pushed, not polled, so a key held when
  focus is lost never gets its release event and stays down forever.

## What `AppMode::Player` turns off

Set once in `main()` before `Engine::Init()`. See the root `CLAUDE.md` and `Engine/CLAUDE.md`
for the full list — the short version is `UndoSystem`, `FileWatcherSystem`,
`AssetMetaService::ScanAndGenerate`, and `AssetManager`'s auto-save. All four write to the
asset tree or exist purely for authoring, and a real install is usually read-only.

## Output layout of a build

```
<GameName>/
  <GameName>.exe        renamed copy of RockEnginePlayer.exe
  game.rock             the BuildConfig
  Domain/               copied wholesale
  python/Lib, python/DLLs
  python3.dll, python313.dll
```

`EngineUtils::GetAssetRoot()` returns the executable's folder as soon as a sibling `Domain/`
exists (else the compiled-in `PROJECT_ROOT`), and `Engine::Init` points `PYTHONHOME` at
`<exeDir>/python` if that exists. Both predate this work; the build pipeline just satisfies
them.

## Dev loop

`build/local/bin/RockEnginePlayer.exe Domain/sandbox/default.scene` — no `game.rock` and no
sibling `Domain/`, so `GetAssetRoot()` falls back to `PROJECT_ROOT` and the player runs
straight against the live source tree. No packaging step. Use this to check a change outside
the editor before doing a real build.
