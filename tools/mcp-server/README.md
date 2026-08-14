# RockEngine MCP server

Lets an MCP client (Claude Code) inspect and drive a **running** RockEngine editor: walk the
scene hierarchy, move objects, edit component properties, enter play mode, trigger builds.

## Shape

```
Claude Code  --stdio-->  server.py  --named pipe-->  Editor process
                        (this dir)                   (Editor/src/mcp/)
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

`../../.mcp.json` registers the server for this repo and already points at
`tools/mcp-server/.venv/Scripts/python.exe`, so the venv above is not optional — the
system interpreter will not have `mcp`. On macOS/Linux change that path to
`.venv/bin/python`. The venv itself is gitignored; recreate it per machine.

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
```

Get an object id from `scene.hierarchy` first.

## Things to know

- **Every result carries `worldMode`.** `"Runtime"` means the edit landed on play mode's
  deep-copied world and is discarded on Stop; a `warning` field says so explicitly.
- **Edits bypass undo.** Like script-driven changes, MCP edits do not enter the Ctrl+Z stack
  — the editor records undo at its own call sites, not inside engine mutators. `destroy_object`
  in particular is not recoverable.
- **Play-mode and build transitions are asynchronous.** `enter_play_mode` and `build_game`
  submit work and return; poll `get_engine_mode` / `get_build_status`.
- **Object ids are stable across the play-mode swap** (the world copy preserves them), so an
  id captured in the editor still resolves after pressing Play.
- **The editor starts with no scene loaded.** `load_scene` is usually the first call; it is
  asynchronous, so poll `list_scenes`.
- **Errors come back as a `{"error", "code"}` payload**, not an MCP protocol error, so a
  failed call reads as content rather than an exception. Codes: `-32001` not found,
  `-32002` build already running, `-32003` Python-side exception, `-32004` wrong mode.
- **The bridge only starts listening once the editor is fully built** (`McpServer::Install()`
  is deliberately the last thing in `Editor::PostInit`). Connecting therefore takes ~7s on a
  cold auto-launch, but the first call already sees the whole AssetManager. Moving that
  install earlier reintroduces a window where the engine answers and every asset list is
  empty.
