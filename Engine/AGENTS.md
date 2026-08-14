# Engine guidance

Before changing files in this subtree, read `Engine/CLAUDE.md` completely. It documents the runtime/container lifecycle, play-mode copying, events, serialization, rendering, audio, jobs, scripting bindings, and engine-specific invariants.

- Do not introduce Qt dependencies into `RockEngineCore`.
- Trace lifecycle, serialization, registration, and `Copy()` implications for new runtime types or state.
- When changing a script-facing API, update and validate both the C++ pybind11 binding and its `Domain/lib/api/` handler.
- Keep process-global hardware/resources outside per-world containers unless ownership genuinely varies by world.
