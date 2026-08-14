# Player guidance

Before changing files in this subtree, read `Player/CLAUDE.md` completely. It documents startup order, SDL/OpenGL hosting, asynchronous scene loading, frame pacing, input translation, and packaged-game layout.

- Keep Player independent of Editor and Qt libraries.
- Set `AppMode::Player` before `Engine::Init()`.
- Preserve Qt-compatible public input codes at the SDL translation boundary.
- Keep engine updates running when rendering is skipped for a minimized window.
