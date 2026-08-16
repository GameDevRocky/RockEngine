# Editor (`RockEngineEditor`)

The Qt6 editor UI. Depends on Engine (`RockEngineCore`); links Qt6 Core/Gui/Widgets/
OpenGLWidgets. CMake `AUTOMOC`/`AUTOUIC`/`AUTORCC` are on — Qt classes with `Q_OBJECT` are
mocced automatically. Headers in `Editor/include/`, sources in `Editor/src/`.

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

## Layout

- **`src/dock-widgets/`** — the dockable panels: `MainWindowGui` (the `QMainWindow` host),
  `HierarchyGui`, `InspectorGui`, `ConsoleGui`, `SceneViewGui`, `GameViewGui`, `FolderViewGui`,
  `FileExplorerGui`, `AiChatGui`, `MenuBar`, `RuntimeBar` (play/pause/stop), `BuildWindow` (File → Build
  Game…, a top-level window rather than a dock).
- **`src/component-widgets/`** — per-component inspector UI pieces: `ComponentHeader`,
  `ObjectHeader`. Dragging a `ComponentHeader` label or non-interactive header surface starts
  the component MIME drag. A drop back in the Inspector commits through
  `GameObject::MoveComponent` and an
  undoable `ComponentOrderCommand`; a drop in AI chat attaches component context.
  Inspector section expansion is editor-only per-session view state keyed by stable object,
  component, and asset IDs, so destructive Inspector rebuilds and component reordering restore
  the prior layout without adding UI fields to scene or asset serialization. Every collapsible
  header reserves a fixed icon slot immediately before its enable toggle; `ObjectHeader` uses the
  shared GameObject icon, while `ComponentHeader` resolves component and asset types through
  `CustomIconProvider` and leaves the slot blank when no mapping exists. The provider also owns
  matching file-suffix mappings so Inspector headers, Folder/File Explorer views, AI references,
  and attachment cards share the same bundled component/asset icon vocabulary.
- **`src/utils/`**:
  - `InspectorVisitor` — an `IVisitor` over `Serializable` that builds property editors from
    an object's reflected fields (the bridge from Engine reflection to Qt widgets).
  - `GizmoUndoBridge` — converts completed Engine gizmo gestures into editor undo commands,
    including `AudioSource` minimum/maximum distance handle drags.
  - `AssetThumbnails` / `AssetPreviewDelegate` — asset-browser previews. The Folder grid's
    delegate bypasses Qt's item-style painter, so it paints the default stylesheet's rounded
    hover/selection bubbles itself rather than relying on `QListView::item` rules.
  - `SceneTree` / `SceneTreeItemDelegate` — hierarchy tree model/rendering.
  - `ImGuiInstance` — ImGui + ImGuizmo for in-viewport gizmos, rendered over the Qt GL widget.
  - `CollapsableWidget`, `EditorUtils`, `MessageGui`.

## Patterns

- GUI singletons use static `Get()` (e.g. `MainWindow::Get()`, `SceneViewGui::Get()`).
- The editor reacts to engine state via the Engine **Observable/Event** system (e.g. play-mode
  enter/exit, selection changes, `ASSET_ADDED_EVENT`) — subscribe to engine events rather than
  polling. See Engine's CLAUDE.md for the event semantics (notably: a callback returning
  `false` auto-unsubscribes).
- Scene/Game views (`SceneViewGui`/`GameViewGui`) derive from `ViewportWidget`, a thin
  `QOpenGLWidget` base that hosts an engine-owned `RenderView` (`EditorRenderView`/
  `GameRenderView`) and hands it an FBO id each frame — the pipeline, passes, and camera all
  live in Engine, not here. See Engine's CLAUDE.md "Rendering" section for the ownership chain.
- **Never `exec()` a dialog.** There are no `QDialog` subclasses here by design — the reason is
  documented on `LoadingOverlay.hpp`: `exec()` spins a nested `QEventLoop`, inside which
  `frameSwapped` still fires → `Editor::FrameTick` → `Engine::Update` → `JobSystem::Pump`,
  re-entering the pump from inside a job step. `QApplication::processEvents()` and
  `QProgressDialog` are out for the same reason. Windows are plain top-level `QWidget`s shown
  with `show(); raise(); activateWindow();` (see `BuildWindow`, `SpriteEditorModal`). The static
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
  schemas. Every run injects the existing `tools/mcp-server/server.py` as the `rockengine` stdio
  server, so live scene changes follow the same bridge and main-thread guarantees as an external
  MCP client. The MCP venv under `tools/mcp-server/.venv/` must be installed.
- New MCP integrations use `component.describe` / `asset.describe` and exact component or resource
  IDs. Property schemas are discoverable and must remain synchronized with Inspector-accessible
  component/resource setters, including enum choices and dynamic ScriptComponent fields or
  Material uniforms. Repeatable components must never be addressed only by owner plus type.
  Component property batches roll back on failure and enter the active container's Undo stack as
  one command; asset setters retain the Inspector's non-undoable, metadata-auto-save behavior.
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
  keyring (`cli_auth_credentials_store="keyring"`) with no plaintext fallback.
- Claude runs through Claude Code in non-interactive JSONL mode. Third-party Claude.ai OAuth is
  intentionally not exposed: Anthropic requires this integration to use a Console API key. The
  key is stored by `tools/ai/credential_store.py` in Windows Credential Manager, macOS Keychain,
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
