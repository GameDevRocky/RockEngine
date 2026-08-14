---
name: verify
description: Build RockEngine with its local MSVC/CMake preset to confirm editor or engine changes compile. Use when verifying RockEngine code changes. This is build-only verification; never launch, run, wait for, or drive the editor or player.
---

# Verify RockEngine

Perform build-only verification. Do not launch `RockEngineLauncher.exe`, `RockEnginePlayer.exe`, or any other engine executable. Do not wait for, automate, or capture screenshots of the editor.

## Build

MSVC headers are not on `PATH` in a bare shell. Enter a Visual Studio developer shell, then build the local preset:

```powershell
$vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
Import-Module "$vsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64"
cmake --build --preset local
```

`Editor/CMakeLists.txt` globs `src/**` with `CONFIGURE_DEPENDS`. A new file can cause a normal `GLOB mismatch!` message and reconfiguration. Do not pipe the build through `Select-String`; use `Select-Object -Last 40` when output must be shortened.

## Finish

Report whether the build succeeded. If it failed, report the failing target and the relevant compiler or linker errors. Do not perform runtime verification.
