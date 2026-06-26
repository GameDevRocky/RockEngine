# RockEngine one-command setup (Windows).
#
#   git clone --recursive https://github.com/GameDevRocky/RockEngine.git
#   cd RockEngine
#   ./setup.ps1
#
# What it does: checks tools -> inits submodules -> finds Qt (an existing install,
# else downloads it via aqtinstall into ./.qt) -> writes CMakeUserPresets.json ->
# configures + builds.
#
# Qt resolution order: -QtDir > a prior ./.qt download > an installed Qt under
# C:\Qt\<ver>\msvc20xx_64 > a fresh aqtinstall download.
#
# Options:
#   -QtDir <path>   Use an existing Qt install instead of auto-detecting/downloading
#                   (the folder containing bin/, lib/cmake/, e.g. C:/Qt/6.11.0/msvc2022_64).
#   -NoRun          Build but do not launch the editor.

[CmdletBinding()]
param(
    [string]$QtDir = "",
    [switch]$NoRun
)

$ErrorActionPreference = "Stop"
Set-Location -Path $PSScriptRoot

# --- Pinned dependency versions (bump here to upgrade everyone consistently) ---
$QT_VERSION   = "6.11.0"
$QT_ARCH      = "win64_msvc2022_64"   # aqt arch id
$QT_DIR_NAME  = "msvc2022_64"         # folder aqt creates under .qt/<ver>/
$PRESET       = "windows-msvc"        # base preset the 'local' user preset inherits

function Require-Tool($name, $hint) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        Write-Error "Missing required tool '$name'. $hint"
    }
}

# Look for an already-installed Qt in the usual places (the official Qt online
# installer drops it under C:\Qt\<ver>\<arch>). Prefer the pinned version, then
# any recent 6.x with an MSVC build. Returns the prefix path, or $null.
function Find-SystemQt($version) {
    $roots = @("C:\Qt", (Join-Path $env:USERPROFILE "Qt"))
    $arches = @("msvc2022_64", "msvc2019_64")
    foreach ($root in $roots) {
        foreach ($arch in $arches) {
            $p = Join-Path $root "$version\$arch"
            if (Test-Path (Join-Path $p "lib\cmake")) { return $p }
        }
    }
    # Fall back to the newest installed 6.x with an MSVC build.
    foreach ($root in $roots) {
        if (-not (Test-Path $root)) { continue }
        $vers = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -match '^6\.' } |
                Sort-Object { [version]($_.Name) } -Descending
        foreach ($v in $vers) {
            foreach ($arch in $arches) {
                $p = Join-Path $v.FullName $arch
                if (Test-Path (Join-Path $p "lib\cmake")) { return $p }
            }
        }
    }
    return $null
}

Write-Host ">> Checking baseline tools..." -ForegroundColor Cyan
Require-Tool git    "Install Git for Windows: https://git-scm.com/download/win"
Require-Tool cmake  "Install CMake >= 3.23: https://cmake.org/download/ (or via Visual Studio)"
Require-Tool python "Install Python 3.10+ (with the 'Development' headers): https://www.python.org/downloads/"

# Visual Studio C++ toolchain (provides cl.exe + the MSVC environment for Ninja).
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "Visual Studio not found. Install Visual Studio with the 'Desktop development with C++' workload."
}
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    Write-Error "No Visual Studio with the C++ toolset found. Add 'Desktop development with C++' in the VS Installer."
}

Write-Host ">> Initializing submodules..." -ForegroundColor Cyan
git submodule update --init --recursive

# --- Resolve Qt: explicit dir > existing .qt download > installed Qt > aqt download ---
$qtDownloadDir = Join-Path $PSScriptRoot ".qt"
$qtAutoPath    = Join-Path $qtDownloadDir "$QT_VERSION\$QT_DIR_NAME"

if ($QtDir -ne "") {
    $qtPrefix = $QtDir
    Write-Host ">> Using provided Qt: $qtPrefix" -ForegroundColor Cyan
} elseif (Test-Path (Join-Path $qtAutoPath "lib\cmake")) {
    $qtPrefix = $qtAutoPath
    Write-Host ">> Reusing previously downloaded Qt: $qtPrefix" -ForegroundColor Cyan
} elseif ($sysQt = Find-SystemQt $QT_VERSION) {
    $qtPrefix = $sysQt
    Write-Host ">> Using installed Qt: $qtPrefix" -ForegroundColor Cyan
} else {
    Write-Host ">> No Qt found locally. Downloading Qt $QT_VERSION ($QT_ARCH) via aqtinstall (first run only)..." -ForegroundColor Cyan
    $venv = Join-Path $PSScriptRoot ".venv-setup"
    if (-not (Test-Path $venv)) { python -m venv $venv }
    $venvPy = Join-Path $venv "Scripts\python.exe"
    & $venvPy -m pip install --quiet --upgrade pip aqtinstall
    # Don't let a flaky download abort the whole script; handle it with guidance.
    & $venvPy -m aqt install-qt windows desktop $QT_VERSION $QT_ARCH -O $qtDownloadDir
    $qtPrefix = $qtAutoPath
    if (-not (Test-Path (Join-Path $qtPrefix "lib\cmake"))) {
        Write-Host ""
        Write-Host "!! Automatic Qt download failed (aqtinstall could not fetch Qt $QT_VERSION)." -ForegroundColor Yellow
        Write-Host "   This can happen when aqt lags behind Qt's newest releases or a mirror is down." -ForegroundColor Yellow
        Write-Host "   Do one of the following, then re-run this script:" -ForegroundColor Yellow
        Write-Host "     1. Install Qt $QT_VERSION (Desktop MSVC 2022 64-bit) with the official"          -ForegroundColor Yellow
        Write-Host "        Qt Online Installer:  https://www.qt.io/download-qt-installer"                -ForegroundColor Yellow
        Write-Host "        It will be auto-detected under C:\Qt\$QT_VERSION\msvc2022_64."                -ForegroundColor Yellow
        Write-Host "     2. Or point at an existing Qt:  ./setup.ps1 -QtDir `"C:/Qt/<ver>/msvc2022_64`"" -ForegroundColor Yellow
        Write-Error "Qt not available. See guidance above."
    }
}

# --- Generate CMakeUserPresets.json (gitignored) so CLI + VS + VS Code all find Qt ---
$qtPrefixJson = ($qtPrefix -replace '\\','/')
Write-Host ">> Writing CMakeUserPresets.json -> $qtPrefixJson" -ForegroundColor Cyan
$userPresets = @"
{
  "version": 6,
  "configurePresets": [
    {
      "name": "local",
      "displayName": "RockEngine (local Qt)",
      "inherits": "$PRESET",
      "cacheVariables": {
        "CMAKE_PREFIX_PATH": "$qtPrefixJson"
      }
    }
  ],
  "buildPresets": [
    { "name": "local", "configurePreset": "local" }
  ]
}
"@
Set-Content -Path (Join-Path $PSScriptRoot "CMakeUserPresets.json") -Value $userPresets -Encoding utf8

# --- Import the MSVC environment so Ninja can find cl.exe from this shell ---
Write-Host ">> Importing MSVC environment (vcvars64)..." -ForegroundColor Cyan
$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^(.*?)=(.*)$') { Set-Item -Path "env:$($matches[1])" -Value $matches[2] }
}

# --- Configure + build ---
Write-Host ">> Configuring (preset: local)..." -ForegroundColor Cyan
cmake --preset local
Write-Host ">> Building (preset: local)..." -ForegroundColor Cyan
cmake --build --preset local

$launcher = Join-Path $PSScriptRoot "build\local\bin\RockEngineLauncher.exe"
Write-Host ">> Done. Launcher: $launcher" -ForegroundColor Green
if (-not $NoRun -and (Test-Path $launcher)) {
    Write-Host ">> Launching..." -ForegroundColor Green
    & $launcher
}
