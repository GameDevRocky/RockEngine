# Editor (`RockEngineEditor`)

The Qt6 editor UI. Depends on Engine (`RockEngineCore`); links Qt6 Core/Gui/Widgets/
OpenGLWidgets. CMake `AUTOMOC`/`AUTOUIC`/`AUTORCC` are on — Qt classes with `Q_OBJECT` are
mocced automatically. Headers in `Editor/include/`, sources in `Editor/src/`.

## Tests

`RockEngineEditorTests` uses GoogleTest with Qt Test helpers. Its process creates one
`QApplication`, then initializes Engine in Editor mode without calling `Editor::Init()`;
the tests exercise widgets and controllers without starting the main window or frame
driver. `SceneEditController` has a constructor accepting explicit tools so its input
routing can be tested without constructing its production gizmo and tilemap tools.
CTest runs the editor suite with Qt's offscreen platform; tests requiring a real GL
viewport belong in a separate integration suite.

## Startup & frame loop

- **`Editor`** singleton (`src/Editor.cpp`) sets up `QApplication`, Fusion style +
  `Domain/lib/assets/styling/default.qss`, and an OpenGL 4.6 **core profile** default surface
  format (depth 24, stencil 8, 4x MSAA, `AA_ShareOpenGLContexts`, `setSwapInterval(1)` for
  vsync). Then runs `MainWindow`.
- **The frame loop is vsync-driven, not timer-driven.** `Editor::SetFrameDriver(QOpenGLWidget*)`
  connects exactly one viewport's `QOpenGLWidget::frameSwapped` signal to `Editor::FrameTick()`
  at a time — **whichever viewport is currently visible**, not always the Scene view.
  `Editor::PostInit` starts the driver on the Scene view (the initially active tab);
  `MainWindowGui`'s `central_tabs` `currentChanged` signal calls `SetFrameDriver` again on every
  tab switch (e.g. `RuntimeBar` switching to the Game tab on play), disconnecting the old
  driver and connecting the new one. Because `setSwapInterval(1)` paces buffer swaps to the
  display refresh, each swap fires `frameSwapped` → `FrameTick()` (`Engine::Get()->Update()`
  then `Editor::Update()`, which `update()`s only the current driver), which requests the next
  repaint → swaps again → re-fires the signal. A self-sustaining loop **locked to the monitor's
  refresh rate**, so FPS equals the display's Hz (e.g. 165 on a 165Hz panel), *not* a fixed 60.
  Time-dependent logic must scale by delta-time or it runs faster on high-refresh displays.
  (Before this, only the Scene view's `frameSwapped` was ever connected, so switching to the
  Game tab — the only way to see the running game — silently dropped the whole engine to the
  watchdog's ~31 FPS for as long as that tab was active. If you ever see the frame loop hardcode
  a single view again, that bug is back.)
- A `QTimer` (~16ms, `Qt::PreciseTimer`) is only a **fallback watchdog**, not the driver: it
  calls `FrameTick()` only when ≥32ms (~2 frames) pass with no swap — e.g. no viewport visible,
  minimized, or occluded — so scripts/logic never freeze. On a healthy loop it never fires.
- `app->exec()` (in `PostInit`) blocks as the app's main loop.
- Entry order from `src/main.cpp`: `Engine::Init → Editor::Init → Engine::PostInit →
  Editor::PostInit` (blocks) → shutdown in reverse.
- Shutdown deletes `MainWindow` before `QApplication`; its owned `QOpenGLWidget`s must release
  their QRhi backing textures and ImGui renderer resources while the shared context group still
  exists. The singleton pointer is cleared through `MainWindow::Destroy()`.

## Layout

- **`src/dock-widgets/`** — the dockable panels: `MainWindowGui` (the `QMainWindow` host),
  `HierarchyGui`, `InspectorGui`, `ConsoleGui`, `SceneViewGui`, `GameViewGui`, `FolderViewGui`,
  `FileExplorerGui`, `AiChatGui`, `SpriteEditor`, `MenuBar`, `RuntimeBar` (play/pause/stop),
  `BuildWindow` (File → Build Game…, a top-level window rather than a dock). The Sprite Editor
  is created lazily and tabbed into the central Scene/Game dock area. Once created, it follows
  texture asset selection and clears to an empty state if its texture is destroyed.
  Grid slicing by cell count or cell size defers sprite creation/removal saves and
  writes the texture descriptor once after the complete Apply operation, including
  Replace existing. Individual canvas edits and deletions still save immediately.
  FolderView's right-click New menu creates a uniquely named folder in the current directory.
  FolderView supports F2 and context-menu rename under `Domain/sandbox`. It preserves
  extensions, moves paired source/meta files together, and updates known metadata paths,
  loaded scene paths, the startup-scene config, and ScriptComponent module references.
  Dragging project files/folders onto a folder cell, an ancestor breadcrumb, or a
  File Explorer directory moves them through the same reference-preserving transaction.
  `ProjectFileDropTarget` resolves and highlights those destinations; external source
  assets retain copy/import semantics. Moves reject collisions and folder cycles,
  and sprite sub-assets remain copy-only reference drags. Moving Python files updates
  serialized/live ScriptComponent module paths; destination path components must be
  valid Python identifiers. Script discovery still scans the fixed `scripts` directory.
  The sandbox root, its `scripts` directory, and `project.build` are fixed paths; bundled
  `Domain/lib` content is outside the rename area.
  Textures with sprites expose a focusable `QPushButton` centered on the cell's right edge,
  without shifting or reserving space in the preview. `FolderAssetView` keeps buttons
  aligned with visible texture cells on scroll, resize, and model changes; their clicks toggle
  expansion by texture ID. `FolderAssetModel` inserts sprite sub-assets after each expanded
  texture in the same wrapping grid. `FolderAssetView` also handles copy-only sprite drags.
  Expanded groups have a shared gray backing.
  All entries use one cell size controlled by the Folder View's bottom-right Size slider
  (24–128 pixel thumbnail bounds). Cell width and height derive from that single value,
  with fixed room for labels. Ctrl+mouse-wheel over the grid updates that same slider.
  Sprite previews fit the shared thumbnail bounds while
  preserving aspect ratio and omitting scene pivot offsets. Source image dimensions
  never enlarge the grid cells. Expansion is remembered per texture for the session.
  Sprite rows select the sprite resource and emit the single/list sprite MIME formats, never
  filesystem URLs. Delete and the context menu remove selected sprite IDs through
  `AssetManager::RemoveSprite`, which updates their texture descriptor without deleting the
  texture file. Mixed sprite/file selections use one asynchronous confirmation, then remove
  files before sprites so a selected texture's children are not removed twice.
  Sprite labels use `GetName()` when nonempty, otherwise `GetID()`.
  Observable subscriptions refresh membership, names, dimensions and previews after asset edits.
- **`src/component-widgets/`** — per-component inspector UI pieces: `ComponentHeader`,
  `ObjectHeader`. Dragging a `ComponentHeader` label or non-interactive header surface starts
  the component MIME drag. A drop back in the Inspector commits through
  `GameObject::MoveComponent` and an
  undoable `ComponentOrderCommand`; a drop in AI chat attaches component context.
  Inspector section expansion is editor-only per-session view state keyed by stable object,
  component, and asset IDs, so destructive Inspector rebuilds and component reordering restore
  the prior layout without adding UI fields to scene or asset serialization.
  Inspector selection coalesces rebuild requests and uses deferred `InspectorVisitor` rows for
  GameObjects, components, and assets. The Inspector creates those rows in short Qt event-loop
  batches and fades each row in as it materializes; deferred component sections and batched
  `ListPropertyWidget` child rows use the same short opacity animation.
  Adding or removing a `ListPropertyWidget` row keeps the other editors in place; value
  refreshes likewise retain existing rows when the list length stays the same or grows.
  Editable lists also have compact bottom-right `+` and `-` controls for appending or removing
  the final item; read-only lists omit them.
  Folder selections prefer those live assets, then fall back to a read-only source preview for
  text files without a resolvable `Serializable` (notably unloaded scenes and Python scripts).
  `SourcePreviewEdit` owns Python/YAML syntax coloring and Ctrl+mouse-wheel font zoom; its scrollbar
  chrome stays hidden while plain wheel input still scrolls, and file previews stay separate from
  engine serialization.
  Pending batches carry a generation and re-resolve component IDs against the active container
  so a changed selection or removed component cannot populate stale UI. Selection notifications
  detach property subscriptions synchronously before scene unload can free their objects.
  Every collapsible header reserves a fixed icon slot immediately before its enable toggle;
  `ObjectHeader` uses the shared GameObject icon, while `ComponentHeader` resolves component and asset types through
  `CustomIconProvider` and leaves the slot blank when no mapping exists. The provider also owns
  matching file-suffix mappings so Inspector headers, Folder/File Explorer views, AI references,
  and attachment cards share the same bundled component/asset icon vocabulary.
- **`src/utils/`**:
  - `InspectorVisitor` — an `IVisitor` over `Serializable` that builds property editors from
    an object's reflected fields (the bridge from Engine reflection to Qt widgets).
  - `GizmoUndoBridge` — converts completed Engine gizmo gestures into editor undo commands,
    including `AudioSource` minimum/maximum distance handle drags.
    ChainCollider point and edge drags, edge insertion, endpoint extension, and point
    deletion also commit a single points edit per gesture.
    In the Scene collider tool, drag a point to move it or an edge to move both
    endpoints. Ctrl+click an edge to insert a point, Shift+drag a head or tail
    point on an open chain to extend it, and Delete to remove the selected point.
    Closed Path and Flip Collision Side are Inspector toggles.
    The Inspector also saves a ChainCollider alpha threshold (0 to 1) and uses it
    when Generate Points from Sprite is clicked.
  - `SceneEditController` receives normalized Scene view input and viewport context,
    gives each press-to-release gesture to one registered editing tool, and draws tool
    previews. Its gizmo adapter forwards modifier and ImGui input to `GizmosManager`;
    the tilemap tool owns paint/erase strokes, previews, and TileMap undo/save.
    SceneViewGui retains viewport navigation and selection fallback.
  - `AssetThumbnails` / `AssetPreviewDelegate` — asset-browser previews. The Folder grid's
    delegate bypasses Qt's item-style painter, so it paints the default stylesheet's rounded
    hover/selection bubbles itself rather than relying on `QListView::item` rules.
    Icons are requested at the display's device-pixel ratio and painted into an explicit
    centered rectangle in logical coordinates, so high-DPI icons fill the same preview area
    as texture thumbnails without shrinking or shifting toward a corner.
    Folder sprites use a separate cache of unpadded previews bounded by the thumbnail size;
    other pickers retain their square previews. Both caches invalidate on sprite and texture
    resource changes.
  - `InlineColorPicker` — the Inspector's collapsible, in-place color editor. It keeps the
    current-color swatch in the value column and expands a full-width row beneath it with
    saturation/value, hue, alpha, RGBA, and hex controls, without creating a dialog or another
    event loop. `PropertyWidgetBase::GetFullRowWidget()` is the optional host contract for that
    secondary detail row.
  - `ObjectRefPropertyWidget` uses that same secondary-row contract: its compact reference field
    stays beside the label, while its searchable thumbnail grid expands beneath it across the
    Inspector. `AssetPickerWidget::setInline()` reuses the popup picker's filtering and selection
    behavior without creating a floating window. Picker cells use an auto-fit grid: the viewport
    chooses the column count from the minimum cell width, then stretches those cells evenly across
    each row. Inline reference picker thumbnails are half the popup picker's size; their 64x68
    minimum cells reserve room for a single elided label and inset selection surface while
    retaining the same responsive column calculation. Reference pickers prepend a
    `None` tile whose empty ID clears the current reference.
    Expanded asset/GameObject pickers subscribe to engine Observable events rather than polling:
    asset add/remove/resource changes, scene/object add, GameObject name/component changes, and
    RuntimeObject shutdown all update the open grid. Subscriptions exist only while expanded and
    are owned by the Qt property widget, so collapse or inspector rebuild removes them. Script
    discovery remains on the existing script file-watcher/Inspector-rebuild path because it scans
    and imports under the GIL.
    Script selectors and nested list/event references must forward `GetFullRowWidget()` so their
    detail grids receive the same full-width row as ordinary reflected references.
    The inline reference picker has the same bottom-right vertical resize grip as the
    multiline text editor. Its bottom-right cell-size slider scales thumbnail cells;
    at the small end it switches to full-width name rows, falling back to IDs for
    unnamed assets or objects. Ctrl+mouse-wheel over either the inline or popup picker updates
    that same slider. Cell size and picker height survive collapsing and
    reopening the same Inspector row.
  - `SceneTree` / `SceneTreeItemDelegate` — hierarchy tree model/rendering.
  - `ImGuiInstance` — ImGui + ImGuizmo for in-viewport gizmos, rendered over the Qt GL widget.
  - `CollapsableWidget`, `EditorUtils`, `MessageGui`.

## Patterns

- GUI singletons use static `Get()` (e.g. `MainWindow::Get()`, `SceneViewGui::Get()`).
- `SettingsGui` mirrors a project-settings browser: its left navigation currently contains only
  Globals, while its right pane dynamically groups every typed value loaded by the engine's
  process-wide `Globals::Get()` singleton. Rows use the same two-column `PropertyFactory` widgets and `PropDesc`
  metadata as the Inspector. It is view-only in play mode; Editor-mode changes are persisted to
  `Domain/lib/configs/Globals.config`. Do not duplicate property definitions in Qt.
- The editor reacts to engine state via the Engine **Observable/Event** system (e.g. play-mode
  enter/exit, selection changes, `ASSET_ADDED_EVENT`) — subscribe to engine events rather than
  polling. See Engine's CLAUDE.md for the event semantics (notably: a callback returning
  `false` auto-unsubscribes).
- Scene/Game views (`SceneViewGui`/`GameViewGui`) derive from `ViewportWidget`, a thin
  `QOpenGLWidget` base that hosts an engine-owned `RenderView` (`EditorRenderView`/
  `GameRenderView`) and hands it an FBO id each frame — the pipeline, passes, and camera all
  live in Engine, not here. See Engine's CLAUDE.md "Rendering" section for the ownership chain.
- `GameViewGui` is also the Qt backend for the engine's process-global `CursorManager`. Custom
  cursors are cropped from Sprite UVs, confinement applies only to the Game view, and locked
  motion is converted into framebuffer-pixel deltas by recentering the native pointer. Escape,
  focus loss, hiding the view, and leaving play mode release any physical mouse grab.
- Plain Scene-view picking is root-first: clicking a rendered child selects its highest
  ancestor, then a consecutive click on that same rendered child drills into the child and keeps
  it selected. The raw hit and prior resolved target live on `SceneViewGui`, not
  `SelectionManager`, so Hierarchy selection, marquee selection, and Ctrl/Shift exact-object
  multi-selection retain their existing semantics.
- Scene-view wheel events update ImGui IO but are never consumed by its event filter; they must
  continue to `SceneViewGui::wheelEvent()` so screen-space gizmo hover cannot interrupt camera zoom.
- **Never `exec()` a dialog.** There are no `QDialog` subclasses here by design — the reason is
  documented on `LoadingOverlay.hpp`: `exec()` spins a nested `QEventLoop`, inside which
  `frameSwapped` still fires → `Editor::FrameTick` → `Engine::Update` → `JobSystem::Pump`,
  re-entering the pump from inside a job step. `QApplication::processEvents()` and
  `QProgressDialog` are out for the same reason. Windows are plain top-level `QWidget`s shown
  with `show(); raise(); activateWindow();` (see `BuildWindow`); editor panels such as the Sprite
  Editor use docks. The static
  `QFileDialog::getExistingDirectory` helper is fine — it runs the platform's own loop, not ours.
- Don't put game/runtime logic here — it belongs in Engine. The Editor only observes and
  drives the engine. Game *build* tooling (`utils/GameBuilder`) is the exception and correctly
  lives here — it is authoring, and the thing it ships is `Player/`.

## AI assistant

- `AiChatGui` is the dockable editor surface; `ai::AiAgentService` owns asynchronous provider
  CLI processes and their JSONL/JSON-RPC event parsing. It never blocks the Qt event loop while an agent is
  working. Partial provider events update one in-progress assistant message, which is finalized
  when the provider process completes. Provider conversation IDs and per-provider model choices
  are stored in `QSettings`. All widget-specific visual styling lives in
  `Domain/lib/assets/styling/ai_assistant.qss`; keep
  `AiChatGui.cpp` limited to structure, object names, and behavior.
- Assistant responses are GitHub-style Markdown rendered by `AiMarkdownMessage`; raw HTML stays
  disabled. Resolvable editor references use `rockengine://` anchors (`object`, `component`,
  `asset`, and project-scoped `file`). Links are resolved against the active container at click
  time, and file targets must remain inside `PROJECT_ROOT`. Never open a model-authored local
  path without that validation.
- Chat attachments preserve their source semantics. Ordinary files are read from disk when the
  draft is sent. Hierarchy GameObjects and Inspector components travel as custom ID MIME payloads,
  while materials/textures/sprites resolve through `AssetManager`; those IDs are serialized from
  the current live in-memory object at send time and include their backing path when one exists.
  A sent user message keeps a display snapshot of those attachments in its collapsible context
  section; later draft changes must not mutate context already shown in the transcript.
  Component drag sources use `kComponentMimeType` in `utils/DragDropMime.hpp`, alongside the
  existing GameObject and sprite MIME contracts. Attachment drops must always finish as a copy so
  the Hierarchy's internal-move model never removes the dragged row.
- The assistant uses the providers' coding-agent CLIs rather than duplicating RockEngine's MCP
  schemas. Every run injects `Editor/src/ai/mcp_server/server.py` as the `rockengine` stdio
  server, so live scene changes follow the same bridge and main-thread guarantees as an external
  MCP client. The MCP venv under `Editor/src/ai/mcp_server/.venv/` must be installed.
- New MCP integrations use `component.describe` / `asset.describe` and exact component or resource
  IDs. Property schemas are discoverable and must remain synchronized with Inspector-accessible
  component/resource setters, including enum choices and dynamic ScriptComponent fields or
  Material uniforms. Repeatable components must never be addressed only by owner plus type.
  Component property batches roll back on failure and enter the active container's Undo stack as
  one command; asset setters retain the Inspector's non-undoable, metadata-auto-save behavior.
- MCP `assets.list` includes TileMaps and TileSets. `TileMapEditor`, `TileMapRenderer`, and
  `TileMapCollider` expose their Inspector fields through the generic component property tools;
  the editor's Grid association and generated collider count are read-only. TileSet definitions
  use `tileset.list_tiles` / `get_tile` / `set_tile` / `remove_tile`; `set_tile` takes a complete
  definition, validates sprites and convex polygons, and saves the asset. Removal requires the
  inline impact clarification because painted cells may lose their resolved sprite/collision.
  `tilemap.get_cells` reads bounded sparse cells; `tilemap.set_cells` edits up to 1000 cells
  through an exact TileMapEditor ID, validates tile IDs against its TileSet, saves the map, and
  records one Undo command. `asset.describe` omits TileMap cell contents to keep large maps
  bounded; use `tilemap.get_cells` for them. Cell authoring and TileSet edits require the editor world.
- AI clarification questions are first-class `AiChatGui` transcript entries presented by
  `AiClarificationWidget`, never top-level windows. Pending cards stay expanded; resolved cards
  collapse to their answer summary and retain a read-only expandable body. The
  `UserClarificationService` emits request/resolution state while `user.clarification_status`
  polling keeps the MCP call asynchronous—never wait with `QDialog::exec()` or
  `QApplication::processEvents()`. Generic ambiguous decisions go through
  `ask_user_clarification`, whose answer bank always adds Other and supports multi-select for
  independent choices. Destructive MCP handlers must analyze their current impact and require a
  scope-bound, one-use editor-owned answer before mutating state. A model-supplied boolean must
  never bypass that authorization.
- OpenAI chat runs through Codex app-server so `item/agentMessage/delta` notifications can be
  rendered live; login still uses the Codex CLI. ChatGPT browser login and
  `codex login --with-api-key` are supported;
  `CODEX_HOME` is isolated under Qt's local app-data directory and Codex is forced to use the OS
  keyring (`cli_auth_credentials_store="keyring"`) with no plaintext fallback. On Windows, the
  app-server launch also explicitly selects Codex's elevated native sandbox because the isolated
  home does not inherit the user's Codex configuration; turns remain scoped to workspace-write.
- Claude runs through Claude Code in non-interactive JSONL mode. Third-party Claude.ai OAuth is
  intentionally not exposed: Anthropic requires this integration to use a Console API key. The
  key is stored by `Editor/src/ai/credential_store.py` in Windows Credential Manager, macOS Keychain,
  or Linux Secret Service, called through `SecureCredentialStore`'s pybind11 bridge. Claude reads
  it through its documented `apiKeyHelper` pipe, not an inherited environment variable. There is
  no `.env`/QSettings/plaintext fallback and this editor-only Python package is not shipped in games.
- Gemini runs through Gemini CLI in non-interactive streaming-JSON mode. AI Studio API keys are
  validated before use and stored in the same native credential vault; the key is injected only
  into the child Gemini process. `GEMINI_CLI_HOME` isolates its settings and sessions under Qt's
  local app-data directory, where RockEngine writes a non-secret trusted `rockengine` MCP entry.
  Embedded Google-account login is not exposed because Gemini's browser login is coupled to its
  interactive terminal UI.
- Provider CLIs are found on `PATH` plus common install locations. `ROCKENGINE_CODEX_CLI` and
  `ROCKENGINE_CLAUDE_CLI` or `ROCKENGINE_GEMINI_CLI` can point to explicit executables. Sign out
  clears the selected provider's resumable session ID and removes its isolated credentials.

## Tilemap authoring

Folder asset selection resolves TileMaps and TileSets through SelectionManager and AssetManager.
Creating a new TileSet seeds it with tile ID 1 and the 10 default neighbor masks used by the
Terrain TileSet, including its Any wildcards. The tile's default sprite and every rule sprite
start empty so they can be assigned in the Inspector.
The TileSet asset Inspector uses the shared list property widget for tiles and rules. Tile rows
show the name on the left and default sprite on the right; selecting one exposes its ID, name,
default sprite asset picker, and default collider (`None`, `Sprite`, `Grid`, or `Custom`) below. Each rule
card stays expanded. Collider and Output selectors occupy the left column; the fixed
sprite preview and 3x3 neighbor grid sit together on the right. Fixed output owns one sprite;
Random and Animation show reorderable sprite lists below Output, and Animation also shows a
min/max speed range there.
Grid feeds full cell bounds to the optional TileMapCollider; Sprite follows the rendered sprite
rectangle/pivot. Custom shows a separate sprite preview with a convex polygon editor for the
default tile or rule. Drag points to move them, double-click an edge to insert a point, and
right-click a point to remove it. The editor enforces 3–8 points and rejects invalid drags.
Points use sprite-local coordinates and are clamped to the sprite rectangle. A sprite larger
than its tile cell can still yield a collider larger than the cell. Random and Animation
rules preview their first sprite; the same normalized polygon scales to each resolved sprite.
The rules list spans the Inspector width, and its entries
show a 3x3 neighborhood grid beside a sprite preview that accepts sprite drops and can be cleared
from its context menu. The list's own controls add, remove, and reorder rules. Each neighbor cell offers Any, Empty (ID 0), or a specific
TileSet tile ID. The TileMapEditor Inspector assigns the TileMap and TileSet. Selecting an
object with TileMapEditor shows a Paint button in the Scene view tool palette. That mode opens
the draggable, collapsible Tile Palette with sprite previews, a None tile for erasing, and a
square paint size in cells using a property widget.
The companion TileMapRenderer Inspector uses the same inline RGBA color picker as SpriteRenderer
to tint the whole map, and chooses a material, Native or Fill Cell sizing, and
sorting layer/order. Its sorting layer uses the same LayerManager-backed dropdown as
SpriteRenderer. `SceneEditController` owns each drag stroke: its tilemap tool samples Grid
cells, fills gaps between mouse samples, previews the brush footprint, and pushes one
`TileStrokeCommand` containing before/after IDs. Left-drag paints the selected palette tile;
right-drag temporarily uses None to erase with the same brush size without changing that selection.
The command saves the TileMap after undo/redo; the tool saves after completing the stroke.
Tilemap objects must sit below a GameObject with a Grid component. The nearest ancestor Grid
supplies cell size and gap; Grids on the tilemap itself or its descendants are ignored. The
tilemap child's world transform controls its placement, so painting, brush previews, rendering,
and picking all follow that same child transform. The Inspector explains a missing ancestor Grid
and refreshes that status when the Grid association changes; the Paint button remains discoverable.
Painting is available only in the editor container when the selected component has an ancestor
Grid, a loaded TileMap and, for nonempty tiles, a TileSet containing the selected ID.
