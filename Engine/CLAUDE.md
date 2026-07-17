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

`Engine::Update()` (called every frame by the editor's QTimer) ticks `activeContainer`.

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

## Scripting — C++ side (pybind11)

- `PYBIND11_EMBEDDED_MODULE(rock_engine, ...)` in `src/bindings/PythonBindings.cpp` exposes
  submodules `core`, `systems`, `components`, `rendering`. Each `Bind*` function lives in its
  own file under `src/bindings/`.
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
