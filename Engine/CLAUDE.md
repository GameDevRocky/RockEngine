# Engine (`RockEngineCore`)

The runtime core. **No Qt here** — pure C++20 + OpenGL/glad, Box2D, yaml-cpp, glm, pybind11.
Headers in `Engine/include/engine/...` (use the `engine/` include prefix), sources mirror them
in `Engine/src/`.

## Runtime model

The whole engine is built from `RuntimeObject`s living inside a `Container`.

- **`RuntimeObject`** (`include/engine/core/RuntimeObject.hpp`) — base for everything with a
  lifecycle. Lifecycle phases are a `State` enum with virtual hooks:
  `Deserialize → Init → PostInit → Awake → Start → Update → Shutdown`. Holds cached pointers
  to the core systems (registry, timeManager, physicsSystem, etc.). Extends `Serializable`.
- **`System`** — a `RuntimeObject` that lives in a Container's system list. One per type per
  container: Registry, TimeManager, InputManager, PhysicsSystem, SceneManager,
  SelectionManager, LayerManager, TagManager, FileWatcherSystem.
- **`Container`** (`include/engine/core/Container.hpp`) — owns a set of Systems, resolved by
  type via `FindSystem<T>()`. Has a `Mode` (Editor / Runtime / Paused) and drives the
  lifecycle across all its systems.
- **`Engine`** (singleton, `src/Engine.cpp`) — owns two containers:
  - `editorContainer` — always live, edit-time world.
  - `runtimeContainer` — a `Copy()` of the editor container, created on `EnterPlayMode()`
    and destroyed on `ExitPlayMode()`. **Play mode = deep-copy the editor world, run it,
    throw it away.** Key invariant: runtime mutations never touch editor state.
  - `activeContainer` points at whichever is current.

`Engine::Update()` (called every frame by the editor's vsync-driven frame loop, or by
`PlayerApp`'s loop in a shipped game) ticks `activeContainer`.

## AppMode — editor process vs. shipped game

`Engine::SetAppMode(AppMode::Editor | AppMode::Player)`, **called before `Init()`** (it decides
which systems get built, so a later call is refused). Defaults to `Editor`, so anything that
never calls it is unchanged. `RockEnginePlayer`'s `main()` is the only caller that sets `Player`.

**This is not `Container::Mode`, and the distinction matters.** `Container::Mode`
(Editor/Runtime/Paused) describes *a world*; `AppMode` describes *the process*. They are
orthogonal — a shipped game's container is still `Mode::Runtime`, reached by the same
`EnterPlayMode()` deep copy the editor's Play button uses. `AppMode` deliberately does **not**
live on `Container`, which would both conflate the two and get it deep-copied by
`Container::Copy()` for a value that can never change. It belongs with
`Renderer`/`AudioEngine`/`GamepadService`: process-global, no per-world identity.

What `AppMode::Player` turns off, and why — every item writes to the asset tree, and a real
install (Program Files, a Steam library) is usually read-only:

- **`UndoSystem`** and **`FileWatcherSystem`** are not added to the container. `FileWatcherSystem`
  tails `Domain/sandbox/scripts` for hot-reload and is the only reason `requirements.txt`
  contains `watchdog` — skipping it is what lets the shipped Python bundle be stdlib-only.
  `SelectionManager` is *kept*: it is nearly free, and `ExitPlayMode` and several bindings
  resolve it.
- **`AssetMetaService::ScanAndGenerate`** is skipped in `Renderer::EnsureInitialized` — metas are
  authoring output, baked at build time.
- **`AssetManager`'s auto-save** is never armed. Otherwise a script nudging a material property,
  or a Font finishing its atlas bake, would rewrite the meta file inside the install.
- **`Console`** gains a stdout/stderr sink, because nothing subscribes to it without the editor
  and messages would otherwise vanish into a map no one reads.

Exposed to scripts as `Application.is_editor` / `Application.is_player`
(`Domain/lib/api/systems/application_system.py`), mirroring Unity's `Application.isEditor`.

## Observable (event system)

Base of almost everything — `Serializable` extends `Observable`, so every RuntimeObject /
System / Component is an event source.
(`include/engine/core/Observable.hpp`, `src/core/Observable.cpp`)

- `Event` is a `std::uint64_t`. Reserved: `ANY_EVENT = 0`, `ALL_EVENT = UINT64_MAX`.
- `CreateEvent()` hands out a process-unique id from an atomic counter (starts at 1). Declare
  events as `static inline const Event FOO_EVENT = Observable::CreateEvent();`.
- A `Callback` wraps a `std::function` returning `bool`, has a unique int id, in two forms:
  `bool()` and `bool(std::any)` (payload-carrying).

**Subscribe** — `Subscribe(lambda, event = ANY_EVENT)` returns an int id (for
`Unsubscribe(id)`). `ANY_EVENT` (default) receives every notify on that object except
`ALL_EVENT` broadcasts; a specific id receives only that event.

**Notify** (`Notify(event = ANY_EVENT, data = {})`):
1. Copy the matching callbacks first (handlers may safely (un)subscribe during dispatch).
2. Matching: `ALL_EVENT` → all buckets; specific event → that event's subscribers **plus**
   `ANY_EVENT` subscribers; `ANY_EVENT` → only `ANY_EVENT` subscribers.
3. Execute each with `data`.
4. **A callback that returns `false` is auto-unsubscribed** — return `true` to persist,
   `false` for a one-shot handler. This is the core idiom.

**Gotchas** — `ANY_EVENT` listeners do **not** hear `ALL_EVENT` broadcasts. Payload is
type-erased `std::any`; sender and handler must agree on the type (commonly the object's
string `id`, e.g. `Notify(SHUTDOWN_EVENT, id)`).

## Serialization

YAML (yaml-cpp) throughout.

- **`Serializable`** (`include/engine/serialization/Serializable.hpp`) — `Serialize()` /
  `Deserialize(node)`, a string `id`, `GetTypeName()`, `Copy()`, and `Accept(IVisitor*)` for
  the visitor pattern (used by the editor inspector). Extends `Observable`.
- **`SerializableFactory`** — name → creator-function registry. Types register so
  `Create("TypeName")` rebuilds them from a `type:` field in YAML.
- **`Registry`** — runtime object registry/lookup.
- Component types register via `RegisterComponentTypes()` (`src/components/ComponentRegistrars.cpp`),
  called in `Engine::Init`.

Scenes are `.scene` YAML. There is no default/sample scene wired up — scenes load only via
drag-and-drop onto the hierarchy panel (`Editor/src/dock-widgets/HierarchyGui.cpp`). The only
scene checked into the repo is `Domain/sandbox/default.scene`.

## Assets

- **`AssetManager`** (singleton, `include/engine/rendering/core/AssetManager.hpp`) — maps of
  Shader / Texture2D / Material / Sprite keyed by id, plus by-name lookups.
  `LoadFromDirectory()` scans recursively for meta files and registers them.
- **`AssetMetaService`** — generates "meta" descriptor files next to source assets:
  - `foo.png` → `foo.png.texture` (Texture2D)
  - `foo.vert` (+`.frag`) → `foo.vert.shader` (Shader)
  - `foo.mat` → Material (already a definition file)
  - `.sprite` files describe Sprites.
  Meta files are YAML carrying a `type:` field for factory dispatch. IDs are the sanitized
  filename stem, UUID suffix only on collision. Safe to run every startup — skips existing.

New asset type → extend `AssetMetaService` (meta convention) and `AssetManager` (a `Load*`
+ storage map + `Get*` accessors). Asset *content* lives in `Domain/` — see its CLAUDE.md.

## Rendering

Rendering lifecycle lives entirely in Engine, **outside any `Container`** — render resources
have no per-world identity, so the editor and runtime containers (which share one screen and
one GL context) must not fight over ownership of a pipeline. The GL context itself still comes
from the editor's Qt `QOpenGLWidget`s (`AA_ShareOpenGLContexts` shares one context group across
all of them), but from the FBO inward, Engine owns everything.

**Ownership chain:** `Renderer` (`src/rendering/Renderer.cpp`, singleton — alongside
`AssetManager`) → owns one `RenderView` per viewport (`src/rendering/views/`:
`EditorRenderView`, `GameRenderView`) → each owns a `RenderPipeline`
(`src/rendering/pipelines/`, a list of `RenderPass`es plus a `RenderTarget`) and a resolved
`RenderCamera`. A Qt widget (`ViewportWidget` in Editor, and its `SceneViewGui`/`GameViewGui`
subclasses) only hosts a `RenderView` and hands it an FBO id + pixel size each frame — it never
touches GL beyond that.

**`Renderer::EnsureInitialized()`** is the one-time bootstrap: `gladLoadGL()`, the
`AssetManager` load, and the shared fullscreen-quad blit resources, all idempotent so whichever
viewport gets a GL context first does the work and every other viewport is a no-op.

**Camera: authored vs. resolved.** `Camera` (`src/components/Camera.cpp`) is a `Component` —
authored settings only (projection, orthoSize, clear flags/color, priority, target aspect,
viewport rect, culling mask, target texture id). Serialized, inspectable, scriptable, and
copyable across the play-mode container swap like any other component. It has **no GL, no
matrices, and no RenderCamera member** — viewport pixel dimensions belong to the view, not the
camera, so two views rendering the same camera at different sizes would otherwise fight over
one set of dims. `RenderCamera` is the **resolved** counterpart: everything a `RenderPass`
actually needs for one view for one frame (pose, matrices, clear settings, culling mask,
viewport rect, target aspect). Owned by a `RenderView`, never serialized. Each frame,
`RenderView::Render()` calls `UpdateCamera()` (a pull, not a push — see below) which resolves
the active camera and calls `Camera::ApplyTo(RenderCamera&)` to write settings + the
GameObject's world pose in. `GameRenderView` resolves via `Camera::GetMain()` (highest-priority
enabled `Camera` on an active GameObject, scanned across loaded scenes — a seam, not a
registered system; see the comment on `GetMain()` before reaching for a `CameraSystem`).
`EditorRenderView` instead owns an `EditorCamera : RenderCamera` — the Scene view's pan/zoom
navigation camera, deliberately **outside the ECS**: never serialized, never deep-copied, never
in the hierarchy.

Sync is a **pull at render time**, not a lifecycle hook: `SceneManager::Update` early-returns
while paused, and Qt can repaint without a `FrameTick` (expose/resize), so pushing camera state
from `Update()`/`LateUpdate()` would go stale. Pulling in `RenderView::Render()` has no
ordering dependency and is always correct — and it's why `Camera` needs no `Update()` override
at all.

**Letterboxing.** A camera's `targetAspect` (`<= 0` means "free" — fill the panel, which is
what the editor view always uses) drives `RenderView::ApplyTargetSizing()`: it fits the largest
rect of that aspect inside the panel, sizes the `RenderPipeline`'s `RenderTarget` to *that* (not
the panel), and `Present()` blits into the centered offset, clearing the rest black. Runs every
frame (not just on resize) since `targetAspect` can change live from the inspector.

- `src/rendering/core/`: Shader, Texture2D, Material, Sprite, Resource, GizmosManager,
  `RenderTarget` (FBO + color texture + depth renderbuffer; owned by `RenderPipeline`).
- `src/rendering/cameras/`: `RenderCamera` (resolved state, see above), `EditorCamera`
  (editor-viewport navigation; pan via `PanByPixels`, zoom-to-cursor via `ZoomAt`).
- `src/rendering/passes/`: ClearPass, GridPass, ScenePass, PickingPass (mouse-pick via id
  buffer), DebugPass — composed by a `RenderPipeline`. A pass may **borrow** shaders/textures
  from `AssetManager` but must never delete them in `Shutdown()` — see the ownership comment on
  `RenderPass`.
- OpenGL 4.6 core profile.

## Audio

`src/audio/`: `AudioEngine` (singleton, owns miniaudio's `ma_engine` — the audio device + mixing
graph) and `AudioClip` (a `Resource`, `.audio` meta next to a wav/mp3/ogg/flac source, same
convention as `.texture`/`.font`; see `AssetMetaService`). Like `Renderer`/`AssetManager`, audio
hardware has no per-world identity, so `AudioEngine` lives **outside any `Container`** rather
than as a per-Container `System` — one physical device shared by editor and runtime containers,
whichever is active. `AudioSource` (`src/components/AudioSource.cpp`) and `AudioListener` are
Components: an `AudioSource` owns a live `ma_sound*` created lazily against `AudioEngine`'s
engine, runtime-only like `RigidBody`'s `b2BodyId` (never serialized/copied — `Copy()` carries
only the authored fields and leaves it null). `spatialBlend` (0 = always centered/full volume, 1
= fully positional) is implemented by lerping the position fed to miniaudio's spatializer
between the listener's own position and the source's true world position, rather than a
separate fake volume/pan blend — real panning and distance attenuation fall out of miniaudio's
own spatializer at any blend in between. The active listener is resolved once per frame in
`Engine::Update` (pull, not push, same rule the render cameras follow) via
`AudioListener::GetMain()`, falling back to `Camera::GetMain()` so positional audio works with
zero setup. miniaudio is vendored as `External/miniaudio/miniaudio.h` (single-header, like
stb/glad); `AudioEngine.cpp` is the one translation unit with `MINIAUDIO_IMPLEMENTATION` defined.

**Two Windows gotchas, both already handled — don't undo them:**
- `AudioEngine::EnsureInitialized()` calls `CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)`
  before touching miniaudio. miniaudio's Win32 context init calls `CoInitializeEx` on the
  *calling* thread with `COINIT_MULTITHREADED` and holds that apartment for the context's life.
  Windows OLE drag-and-drop (`RegisterDragDrop` for drop targets, `DoDragDrop` under every
  `QDrag::exec()`) needs the GUI thread **STA**. Whoever touches COM first wins the apartment,
  and if miniaudio wins, Qt's `OleInitialize()` fails with `RPC_E_CHANGED_MODE` and **all editor
  drag-and-drop silently dies** — Folder-view assets, the sprite hover column, Hierarchy rows —
  while everything else keeps working, with no error anywhere. Claiming STA first makes us agree
  with Qt rather than race it, so it does not matter whether Qt initializes OLE eagerly (at
  `QApplication` construction) or lazily (at first drop-target registration). Do **not** "fix"
  this by reordering init instead — the ordering is not knowable from outside Qt.
- `miniaudio.h` drags in `<windows.h>`, whose `CreateEvent` macro rewrites every
  `Observable::CreateEvent()` parsed after it into `CreateEventA` — a link error pointing at
  unrelated static Event initializers. `AudioEngine.cpp` `#undef`s it right after the include.

## Input & gamepads

Keyboard/mouse and gamepads reach the engine by **opposite routes**, and the difference drives
most of the design here.

- **Keyboard/mouse is pushed in.** Engine has no windows, so `SceneViewGui`/`GameViewGui`
  forward Qt events into `InputManager::SetKeyState` / `SetMouseButtonState` /
  `SetMousePosition` (raw `Qt::Key` / `Qt::MouseButton` ints — there is no engine-side keycode
  enum). Input therefore only flows while a viewport has Qt focus.
- **Gamepads are polled.** `GamepadService` (`src/input/GamepadService.cpp`, singleton) asks the
  OS directly once per frame. No Qt, no focus, no events.

**`GamepadService` lives outside any `Container`**, alongside `Renderer`/`AssetManager`/
`AudioEngine` and for the identical reason: a controller is hardware, and hardware has no
per-world identity. There is one DualSense on the desk whether the editor or runtime container
is ticking, and deep-copying it into play mode would mean two objects fighting over one device
handle. `InputManager` (which *is* a per-Container `System`) reads the service each frame and
derives the pressed/released **edges**, so scripts still get container-scoped input off one
shared device — the same split as `AudioEngine` (global device) vs `AudioSource` (per-world
component).

**SDL3 is the backend**, vendored as `External/SDL`. `SDL_VIDEO` **used to be OFF**, and the
comment block in `External/CMakeLists.txt` used to call that the whole point: the video code was
never compiled, so SDL *could not* create a window even by accident. **That is no longer true** —
`RockEnginePlayer` (see `Player/CLAUDE.md`) needs exactly that capability, so video and OpenGL
are compiled in. The invariant survives as a rule rather than an impossibility:

> Only `RockEnginePlayer` calls `SDL_INIT_VIDEO`. The editor process calls `SDL_INIT_GAMEPAD`
> and nothing else, and Qt still owns every window in the editor.

If `SDL_INIT_VIDEO` or `SDL_CreateWindow` ever becomes reachable from `Editor/` or
`RockEngineLauncher`, that rule is broken and two window systems are fighting over one process.
`SDL_GPU`/`RENDER`/`CAMERA` are pinned **off** explicitly — they are `cmake_dependent_option`s on
`SDL_VIDEO` and would otherwise switch themselves on now; we render through glad against GL 4.6.
`SDL_AUDIO` is off too — miniaudio owns audio. `HIDAPI`/`SENSOR`/`POWER`/`HAPTIC` stay **on**, and
that is what supplies the PlayStation feature set (gyro, accelerometer, touchpad fingers,
lightbar, battery); turn any of them off and a DualSense silently degrades to a generic pad. Qt6
dropped QtGamepad and Engine must not depend on Qt anyway, which is why an external library was
needed at all.

Details worth knowing before changing any of it:

- **Poll order is a correctness constraint.** `GamepadService::Update()` runs in
  `Engine::Update()` **before** `activeContainer->Update()`, because `InputManager::Update()`
  derives this frame's edges from it. Polling after the container tick puts a one-frame lag on
  every controller input.
- **`GamepadTypes.hpp` includes no SDL header.** SDL is an implementation detail of
  `GamepadService.cpp`; letting `SDL_gamepad.h` leak into engine headers would drag it into
  every binding and script-facing TU. `GamepadService.hpp` stores pads as `void*` for the same
  reason.
- **`GamepadButton`/`GamepadAxis` mirror SDL's enum ordering** so translation is a cast, not a
  switch. `GamepadService.cpp` `static_assert`s that correspondence — an SDL upgrade that
  reorders its enum breaks the build there instead of silently swapping Circle for Cross.
- **Buttons are named by position** (`South`/`East`/`West`/`North`), the only naming that stays
  true across vendors. `gamepad_system.py` adds `CROSS`/`CIRCLE`/`SQUARE`/`TRIANGLE` and
  `A`/`B`/`X`/`Y` aliases so script code reads naturally without the engine picking a side.
- **Stick Y is flipped once, in `ReadPad`.** SDL reports Y-down; a Y-up 2D engine wants "push
  up, get +1". Touchpad Y is flipped to match.
- **Deadzone is radial and lives in `InputManager`**, not the service — the service reports raw
  hardware. Per-axis deadzoning is deliberately avoided (it makes diagonals reach further than
  cardinals).
- **Focus gating.** A polled pad has no focus concept, so without this a game left in play mode
  keeps responding while you work in another app. `Editor::Init` connects Qt's
  `applicationStateChanged` to `GamepadService::SetApplicationFocused`. While unfocused every
  `GetState()` returns zeroes and rumble is cut, but devices stay open so nothing is
  re-enumerated on the way back. `IsConnected()` still answers honestly — "is a pad plugged in"
  is not input, and a controller icon in the UI should not flicker when you alt-tab.
- **Rumble outlives its owner.** The motors latch in the controller's own hardware, so anything
  that ends a session has to zero them: `ExitPlayMode`, `Engine::Shutdown`, focus loss, and
  `GamepadService::Shutdown` all do. A script cannot leave a pad buzzing on the desk.
- **COM apartment.** `GamepadService::EnsureInitialized` claims `COINIT_APARTMENTTHREADED`
  before `SDL_Init`, exactly as `AudioEngine` does and for the same drag-and-drop reason (see
  the Audio section above). Both claim STA defensively, so the ordering between audio, gamepads
  and Qt startup does not matter. Don't "fix" it by reordering init.
- **`InputManager::Copy()` seeds pad buttons from live hardware** into both the current and
  previous buffers, so entering play mode while holding a button reads as *down* but never fires
  a phantom *press* edge on the runtime container's first frame.
- **Steam Input** hides the physical pad behind a virtual one when a game runs under Steam. SDL
  handles that correctly; `GamepadState::steamHandle` is non-zero when it is in play, which is
  the signal that remapping is Steam's job and the game should not offer its own.

## Scripting — C++ side (pybind11)

- `PYBIND11_EMBEDDED_MODULE(rock_engine, ...)` in `src/bindings/PythonBindings.cpp` exposes
  submodules `core`, `systems`, `components`, `rendering`, `audio`. Each `Bind*` function lives
  in its own file under `src/bindings/`.
- The embedded interpreter is started in `Engine::Init` (`scoped_interpreter` +
  `gil_scoped_release`).
- Adding a new C++ type to scripting: write a `Bind<Type>` in a bindings file, declare + call
  it in `PythonBindings.cpp`, then add the matching Python handler under `Domain/lib/api/`
  (keep the two in sync).

## When changing Engine code

- New serializable type → register with `SerializableFactory` / the component registrars,
  implement `Serialize`/`Deserialize`, give it a `GetTypeName()`.
- Anything that must survive play mode → make it copyable via `Copy()` (play mode deep-copies
  the editor container).
- Decoupled notifications → use the Observable/Event pattern, not direct coupling.
- Singletons expose a static `Get()`.
