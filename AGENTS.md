# RockEngine Codex Guide

RockEngine is a cross-platform 2D game engine: a C++20/OpenGL/Box2D runtime, a Qt6 editor, an SDL3 standalone player, and an embedded-Python gameplay layer.

## Start here

- Read `CLAUDE.md` completely before making repository-wide changes. It is the canonical architecture and conventions guide retained for compatibility with both Claude Code and Codex.
- When working under `Engine/`, `Editor/`, `Player/`, or `Domain/`, also read the nearest scoped `AGENTS.md` and the sibling `CLAUDE.md` it names.
- Use `rg` and `rg --files` for navigation. Search callers, serialization, bindings, and copy paths before changing a public engine concept.
- Treat `External/` as third-party code. Do not modify it unless the user explicitly requests a dependency patch.
- Preserve unrelated working-tree changes. Generated `__pycache__` and `.pyc` files are not source changes.

## Build and verification

- Normal bootstrap: `./setup.ps1` on Windows or `./setup.sh` on macOS/Linux.
- Normal incremental build: `cmake --build --preset local` after a configured checkout.
- Build output is under `build/local/bin/`.
- For editor or engine changes with a visible runtime surface, use the repo-local `$verify` skill in `.agents/skills/verify`.
- Prefer focused validation first, then build the affected targets. Report anything not run and why.

## Cross-layer invariants

- Keep `Engine/` Qt-free. The editor owns Qt; the player owns SDL.
- Play mode deep-copies the editor world. Runtime state that must survive the copy needs correct `Copy()` behavior.
- Keep Python handlers in `Domain/lib/api/` synchronized with their pybind11 bindings in `Engine/src/bindings/`.
- Use the Observable/Event system for cross-layer notifications. Persistent callbacks return `true`; returning `false` auto-unsubscribes.
- Keep game/runtime logic in Engine and authoring UI in Editor.

## Documentation upkeep

- `CLAUDE.md` and scoped `CLAUDE.md` files remain the detailed source of truth. Update the relevant one when architecture, workflows, or non-obvious invariants change.
- Update `AGENTS.md` only when Codex routing, validation expectations, or concise high-priority rules change.
