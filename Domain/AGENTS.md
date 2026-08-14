# Domain guidance

Before changing files in this subtree, read `Domain/CLAUDE.md` completely. It documents the Python API layout, asset conventions, sandbox project, scripting lifecycle, and C++ binding relationship.

- Keep handlers thin; engine behavior belongs in C++.
- Keep each Python API change synchronized with `Engine/src/bindings/`.
- Preserve source-asset/meta-file pair conventions.
- Do not treat `__pycache__` or `.pyc` files as source.
