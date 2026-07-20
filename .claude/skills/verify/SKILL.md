---
name: verify
description: Build, launch and drive the RockEngine Qt editor to observe a change working end-to-end. Use when verifying editor or engine changes that have a visible runtime surface.
---

# Verifying RockEngine

The surface is the Qt editor GUI (`build/local/bin/RockEngineLauncher.exe`). Drive it with
real mouse/keyboard input and capture screenshots — there is no scripting hook into the editor.

## Build

MSVC headers are not on PATH in a bare shell; enter a VS dev shell first:

```powershell
$vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
Import-Module "$vsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64"
cmake --build --preset local
```

`Editor/CMakeLists.txt` globs `src/**` with `CONFIGURE_DEPENDS`, so new files need no build-file
edit — but the glob recheck prints "GLOB mismatch!" and reconfigures, which is normal.

Do **not** pipe the build through `Select-String`: it swallows the output so a successful build
looks like it did nothing. Use `Select-Object -Last 40`.

## The DPI trap (read this first)

The dev display is **2560x1600 at 200% scaling**. `SetCursorPos` always takes *physical* pixels,
but a DPI-unaware process sees a 1280x800 logical desktop and its screenshots come back scaled —
so every click lands at **half** the intended position and silently does nothing.

Call `SetProcessDPIAware()` before any UI/Graphics call in each PowerShell process (each tool
invocation is a fresh process, so put it at the top of the helper script you dot-source).

A ready-made helper with `Focus-Win` / `Click` / `Drag` / `Keys` / `ShotAll` / `ShotRegion` is
worth rebuilding in the scratchpad; see the pattern in this repo's verification history.

## Launching and loading a scene

```powershell
$p = Start-Process -FilePath "build\local\bin\RockEngineLauncher.exe" -PassThru
Start-Sleep -Seconds 12          # GL + Python interpreter startup
```

**File > Open Scene is one of MenuBar's unconnected signals, and double-clicking a `.scene` in
Folder View opens it in VS Code, not the editor.** The only code path that calls
`SceneManager::LoadScene` is `HierarchyGui::dropEvent`, so the way to load a scene is to
**drag `default.scene` from Folder View onto the Hierarchy panel**:

```powershell
Drag 499 1229 211 512    # default.scene icon -> Hierarchy body (physical px, maximized window)
```

The drag must move in steps with small sleeps or Qt never starts the drag.

Check the Hierarchy panel first, though: the scene sometimes comes up already loaded on a fresh
launch. The mechanism isn't obvious from the code (drop handling is the only `LoadScene` caller),
so screenshot before dragging rather than assuming either way — a second load would add a
duplicate scene tree.

## Screenshot gotchas

- Qt menu popups are separate top-level windows — a window-rect capture misses them. Always use
  a full virtual-screen capture and crop afterwards.
- When a menu popup is open, the capture often comes back with the main window black and only the
  popup drawn, positioned at the **top-left** of the frame. Crop `0,0,900,220` to read it.
- Opening a menu by click is flaky; if the crop is blank, retry (Esc, neutral click, click again).
  `Alt+E` does **not** work — the menus define no mnemonics.

## Useful coordinates (maximized, physical px)

| Target | Coords |
|---|---|
| Edit menu | `152, 84` |
| Play/Stop button | `1175, 156` |
| Scene header row ("Default") | `130, 273` (right-click for Save Scene / New GameObject) |
| Save Scene menu item | `223, 302` |
| New GameObject menu item | `256, 398` |
| First hierarchy root row | `179, 326` |
| Inspector Rotation field | `2272, 547` |

## The strongest evidence: diff the .scene file

The Hierarchy screenshot proves the UI updated; the serialized scene proves the *engine* did.
Right-click the scene header → Save Scene writes `Domain/sandbox/default.scene`, then diff it.
This is how to verify id preservation, ordering, and component values in one shot:

```bash
grep -o 'id: [0-9a-f]*' file | sort -u    # compare id sets across checkpoints
git diff Domain/sandbox/default.scene      # ordering-sensitive
```

**Restore it afterwards**: `git checkout -- Domain/sandbox/default.scene`.

Known pre-existing quirk: a plain load→save round trip changes two float values by 1 ULP
(`Ground Check` localPosition, one localScale). Scene load/save is not bit-idempotent. Don't
chase it as a regression — reproduce it with a no-edit load→save before blaming your change.
The engine does **not** drift transforms while idle; two saves 6s apart are byte-identical.
