# Editor guidance

Before changing files in this subtree, read `Editor/CLAUDE.md` completely. It documents Qt startup, the vsync-driven frame loop, panel layout, engine-event integration, viewport ownership, and modal-window constraints.

- Keep gameplay/runtime behavior out of Editor; Editor observes and drives Engine.
- Do not introduce nested Qt event loops (`QDialog::exec`, `QApplication::processEvents`, or `QProgressDialog`).
- Preserve the active-viewport `frameSwapped` frame driver and its watchdog semantics.
- Use the `$verify` skill for changes with a visible editor surface.
