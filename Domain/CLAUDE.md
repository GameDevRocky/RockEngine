# Domain

Game-side content. **Not compiled** — this is shipped data + the Python scripting API.
Loaded at runtime relative to `PROJECT_ROOT` (the source dir, baked in at build time).

## Layout

- **`lib/api/`** — the user-facing Python scripting API. Mirrors the C++ binding submodules:
  - `components/` — component handlers (`transform_handler`, `sprite_renderer_handler`,
    `rigidbody_handler`, collider handlers, `component_handler`, and
    `scriptable_component_handler` — the base class users subclass).
  - `core/` — `gameobject_handler`.
  - `rendering/` — `sprite_handler`, `material_handler`.
  - `systems/` — `input_system`, `gamepad_system`, `physics_system`, `time_system`,
    `console_system`, `debug_draw`.
- **`lib/assets/`** — textures, sprites, icons, fonts, shaders, `styling/default.qss` (the Qt
  editor stylesheet). Each source asset has a sibling meta file (e.g. `foo.png` +
  `foo.png.texture`) — see Engine's CLAUDE.md for the meta convention.
- **`lib/configs/`** — scenes/configs, incl. `Sample_Scene.yaml` (the default scene).
- **`sandbox/`** — an example game project: `scripts/`, scenes (`.scene`), sprites.

## Scripting model

The Python API wraps the embedded `rock_engine` C++ module. Each Python handler is a thin
proxy holding an object id and forwarding to the C++ binding.

- **`ScriptableComponent`** (`lib/api/components/scriptable_component_handler.py`) is the base
  class user scripts subclass. It provides:
  - Unity-like lifecycle hooks: `awake`, `start`, `update`, `fixed_update`, `late_update`.
  - Collision/trigger callbacks: `on_collision_enter/exit`, `on_trigger_enter/exit`,
    `on_shutdown`.
  - `instantiate(name)` — spawn a GameObject in the same scene.
  - A **coroutine** system: `start_coroutine(gen)` + `yield WaitForSeconds(...)`. Coroutines
    are ticked automatically — `__init_subclass__` wraps the update methods to call
    `_tick_coroutines()`; if a subclass defines no update method, a minimal one is injected.
- User scripts do `from Domain import *` and subclass `ScriptableComponent`. Example:
  `sandbox/scripts/BallScript.py` (animates a sprite via a coroutine).

## When changing Domain code

- A Python handler must stay in sync with its C++ binding (`Engine/src/bindings/`). Adding a
  method on the Python side that calls into C++ requires the corresponding `Bind*` to expose
  it. See Engine's CLAUDE.md.
- Don't add engine logic here — handlers are thin proxies; the real work lives in C++.
- `__pycache__/*.pyc` are build artifacts; ignore them in git status.
