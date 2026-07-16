# Editor (`RockEngineEditor`)

The Qt6 editor UI. Depends on Engine (`RockEngineCore`); links Qt6 Core/Gui/Widgets/
OpenGLWidgets. CMake `AUTOMOC`/`AUTOUIC`/`AUTORCC` are on — Qt classes with `Q_OBJECT` are
mocced automatically. Headers in `Editor/include/`, sources in `Editor/src/`.

## Startup & frame loop

- **`Editor`** singleton (`src/Editor.cpp`) sets up `QApplication`, Fusion style +
  `Domain/lib/assets/styling/default.qss`, and an OpenGL 4.6 **core profile** default surface
  format (depth 24, stencil 8, 4x MSAA, `AA_ShareOpenGLContexts`, `setSwapInterval(1)` for
  vsync). Then runs `MainWindow`.
- **The frame loop is vsync-driven, not timer-driven.** `Editor::PostInit` connects the Scene
  view's `QOpenGLWidget::frameSwapped` signal to `Editor::FrameTick()`. Because `setSwapInterval(1)`
  paces buffer swaps to the display refresh, each swap fires `frameSwapped` → `FrameTick()`
  (`Engine::Get()->Update()` then `Editor::Update()`), which requests the next repaint → swaps
  again → re-fires the signal. A self-sustaining loop **locked to the monitor's refresh rate**,
  so FPS equals the display's Hz (e.g. 165 on a 165Hz panel), *not* a fixed 60. Time-dependent
  logic must scale by delta-time or it runs faster on high-refresh displays.
- A `QTimer` (~16ms, `Qt::PreciseTimer`) is only a **fallback watchdog**, not the driver: it
  calls `FrameTick()` only when ≥32ms (~2 frames) pass with no swap — e.g. Scene view hidden,
  minimized, or occluded — so scripts/logic never freeze. On a healthy loop it never fires.
- `app->exec()` (in `PostInit`) blocks as the app's main loop.
- `Editor::Update()` refreshes `SceneViewGui` and `GameViewGui` each frame.
- Entry order from `src/main.cpp`: `Engine::Init → Editor::Init → Engine::PostInit →
  Editor::PostInit` (blocks) → shutdown in reverse.

## Layout

- **`src/dock-widgets/`** — the dockable panels: `MainWindowGui` (the `QMainWindow` host),
  `HierarchyGui`, `InspectorGui`, `ConsoleGui`, `SceneViewGui`, `GameViewGui`, `FolderViewGui`,
  `FileExplorerGui`, `MenuBar`, `RuntimeBar` (play/pause/stop).
- **`src/component-widgets/`** — per-component inspector UI pieces: `ComponentHeader`,
  `ObjectHeader`.
- **`src/utils/`**:
  - `InspectorVisitor` — an `IVisitor` over `Serializable` that builds property editors from
    an object's reflected fields (the bridge from Engine reflection to Qt widgets).
  - `AssetThumbnails` / `AssetPreviewDelegate` — asset-browser previews (current branch focus).
  - `SceneTree` / `SceneTreeItemDelegate` — hierarchy tree model/rendering.
  - `ImGuiInstance` — ImGui + ImGuizmo for in-viewport gizmos, rendered over the Qt GL widget.
  - `CollapsableWidget`, `EditorUtils`, `MessageGui`.

## Patterns

- GUI singletons use static `Get()` (e.g. `MainWindow::Get()`, `SceneViewGui::Get()`).
- The editor reacts to engine state via the Engine **Observable/Event** system (e.g. play-mode
  enter/exit, selection changes, `ASSET_ADDED_EVENT`) — subscribe to engine events rather than
  polling. See Engine's CLAUDE.md for the event semantics (notably: a callback returning
  `false` auto-unsubscribes).
- Scene/Game views own a Qt OpenGL widget; the engine's render passes draw into their context.
- Don't put game/runtime logic here — it belongs in Engine. The Editor only observes and
  drives the engine.
