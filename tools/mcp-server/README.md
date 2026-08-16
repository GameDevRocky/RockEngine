# RockEngine MCP server

Lets an MCP client inspect and drive a **running** RockEngine editor: walk the scene hierarchy,
move objects, edit component and asset properties, inspect the viewport/console, enter play mode,
and trigger builds. Clients include Claude
Code, Codex, Gemini CLI, and the editor's built-in AI Assistant dock.

## Shape

```
Claude Code / Codex / Gemini CLI / AI Assistant
                 --stdio--> server.py --named pipe--> Editor process
                             (this dir)                (Editor/src/mcp/)
```

`server.py` holds no engine logic. It declares the MCP tools and forwards each call as one
line of JSON-RPC 2.0 to `RockEngine.McpBridge.v1`, a `QLocalServer` living inside the editor.
The engine half runs its handlers on the Qt main thread between frames, so they can touch
`Registry`/`Observable`/Python directly with no locks. See `Editor/include/mcp/McpServer.hpp`
for that reasoning.

Two tiers rather than one because MCP's stdio transport requires the *client* to spawn the
server, but the editor is a GUI app the user launches by hand. If the editor is not running
when a tool is called, `editor_launcher.py` starts it and the bridge retries with backoff.

## Setup

```sh
python -m venv .venv
.venv/Scripts/python.exe -m pip install -r requirements.txt   # Windows
# .venv/bin/python -m pip install -r requirements.txt          # macOS / Linux
```

`../../.mcp.json` registers the server for Claude Code and already points at
`tools/mcp-server/.venv/Scripts/python.exe`, so the venv above is not optional — the
system interpreter will not have `mcp`. On macOS/Linux change that path to
`.venv/bin/python`. The venv itself is gitignored; recreate it per machine.

The built-in AI Assistant constructs an equivalent absolute MCP configuration for all provider
CLIs at runtime, choosing `Scripts/python.exe` on Windows and `bin/python` on macOS/Linux. It does
not modify either provider's global MCP configuration.

Requires **mcp 2.x**: `server.py` imports `MCPServer` from `mcp.server.mcpserver`, which
in 1.x was `FastMCP` in `mcp.server.fastmcp`. A 1.x install fails at import.

Verified on Python 3.14 with mcp 2.0.0.

Environment overrides:

| Variable | Purpose |
|---|---|
| `ROCKENGINE_EDITOR_EXE` | Full path to the editor exe, for a non-default build location |
| `ROCKENGINE_ROOT` | Repo root, if this tree is not at `<root>/tools/mcp-server` |

Default exe path is `<repo>/build/local/bin/RockEngineLauncher.exe`.

## Testing without an MCP client

`tests/raw_pipe_smoke_test.py` talks to the pipe directly, so a failure is unambiguously in
the bridge rather than in MCP framing. Launch the editor, then:

```sh
python tests/raw_pipe_smoke_test.py                                   # ping
python tests/raw_pipe_smoke_test.py scene.hierarchy
python tests/raw_pipe_smoke_test.py transform.set_position '{"id":"<goid>","x":3,"y":1}'
python tests/property_coverage_test.py                              # editor not required
```

Get an object id from `scene.hierarchy` first.

## Discoverable properties

Use `get_capabilities`, then `list_components` or `describe_component`. Components are
addressed by their exact component id—not just by owner and type—so multiple colliders,
joints, scripts, and audio sources on one object remain unambiguous. Every property reports
its current value, data type, writeability, enum choices, numeric constraints, and reference
kind. `set_component_properties` is atomic and records one Undo operation; failed batches roll
back. `describe_asset` and the corresponding asset property tools provide the same contract for
sprites, materials (including active shader uniforms), textures, fonts, shaders, and audio clips.

The old object/type-specific tools remain as compatibility conveniences. New clients should
prefer the discoverable exact-id surface.

## User clarifications

`ask_user_clarification` adds an answer-bank bubble to the AI Assistant thread and waits for the
user's answer. Use it before acting when required details are missing, several materially different
interpretations are plausible, or a requested change has important effects beyond the named
target. Every prompt includes **Other** with free-form text. Set `allow_multiple=true` only for
independent choices that can be applied together; mutually exclusive next actions use radio
buttons.

The editor also enforces clarification for destructive structural operations. Removing a
component reports known cascades and behavior changes—for example, removing a `RigidBody` also
removes its colliders and attached/connected joints. Destroying a GameObject reports descendants,
component count, and external dependent joints. The user may approve, choose a safer alternative,
inspect affected IDs, cancel, or provide another instruction. Once resolved, the bubble collapses
to an answer summary and can be expanded to inspect the complete read-only question and choices.
Approval is an editor-owned, scope-bound, one-use request ID; the legacy `confirm` argument cannot
bypass the answer bank.

The editor emits the inline transcript item and `user.clarification_status` is polled by the stdio
wrapper. Do not replace this with a modal dialog, `QDialog::exec()`, or
`QApplication::processEvents()`—a nested event loop could re-enter the engine frame/job pump from
an MCP handler.

## Things to know

- **Every result carries `worldMode`.** `"Runtime"` means the edit landed on play mode's
  deep-copied world and is discarded on Stop; a `warning` field says so explicitly.
- **Generic component property edits are undoable.** A successful batch is one Ctrl+Z entry.
  Asset edits follow the Inspector's existing non-undoable auto-save behavior. Structural legacy
  edits remain non-undoable; `destroy_object` and `remove_component` require an in-editor impact
  review and explicit user choice.
- **Play-mode and build transitions are asynchronous.** `enter_play_mode` and `build_game`
  submit work and return; poll `get_engine_mode` / `get_build_status`.
- **Object ids are stable across the play-mode swap** (the world copy preserves them), so an
  id captured in the editor still resolves after pressing Play.
- **The editor starts with no scene loaded.** `load_scene` is usually the first call; it is
  asynchronous, so poll `list_scenes`.
- **Errors come back as a `{"error", "code"}` payload**, not an MCP protocol error, so a
  failed call reads as content rather than an exception. Codes: `-32001` not found,
  `-32002` build already running, `-32003` Python-side exception, `-32004` wrong mode, and
  standard `-32602` for invalid/missing parameters.
- **The bridge only starts listening once the editor is fully built** (`McpServer::Install()`
  is deliberately the last thing in `Editor::PostInit`). Connecting therefore takes ~7s on a
  cold auto-launch, but the first call already sees the whole AssetManager. Moving that
  install earlier reintroduces a window where the engine answers and every asset list is
  empty.
