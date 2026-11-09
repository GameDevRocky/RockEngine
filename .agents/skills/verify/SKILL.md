---
name: verify
description: Build RockEngine and run its unit tests with the local preset. Use when verifying RockEngine code changes. Never launch, run, wait for, or drive the editor or player.
---

# Verify RockEngine

Build the changed targets and run the relevant unit tests. Do not launch `RockEngineLauncher.exe` or `RockEnginePlayer.exe`. Do not wait for, automate, or capture screenshots of the editor.

## Build

MSVC headers are not on `PATH` in a bare shell. Enter a Visual Studio developer shell, then build the local preset:

```powershell
$vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
Import-Module "$vsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64"
cmake --build --preset local
```

`Editor/CMakeLists.txt` globs `src/**` with `CONFIGURE_DEPENDS`. A new file can cause a normal `GLOB mismatch!` message and reconfiguration. Do not pipe the build through `Select-String`; use `Select-Object -Last 40` when output must be shortened.

## Unit tests

After building, run the affected suite from the same developer shell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./run-tests.ps1 engine   # Engine changes
powershell -NoProfile -ExecutionPolicy Bypass -File ./run-tests.ps1 editor   # Editor changes
powershell -NoProfile -ExecutionPolicy Bypass -File ./run-tests.ps1          # Changes spanning both suites
```

The process-scoped execution-policy flag is needed on Windows machines that block local PowerShell scripts. On macOS/Linux use the corresponding `./run-tests.sh` commands. The scripts build their test targets before executing them. If the full build cannot relink a running launcher, still run the relevant test command; test targets build independently. Use a focused test selector first when it helps diagnose a failure, then run the affected suite.

## Finish

Report build and unit-test results separately. For failures, report the target or test and relevant error. Do not run the editor or player for verification.
