#!/usr/bin/env bash
# RockEngine one-command setup (macOS / Linux).
#
#   git clone --recursive https://github.com/GameDevRocky/RockEngine.git
#   cd RockEngine
#   ./setup.sh
#
# What it does: checks tools -> inits submodules -> finds Qt (an existing install,
# else downloads it via aqtinstall into ./.qt) -> writes CMakeUserPresets.json ->
# configures + builds.
#
# Qt resolution order: --qt-dir > a prior ./.qt download > an installed Qt under
# ~/Qt/<ver>/<arch> (or /opt/Qt, /Applications/Qt) > a fresh aqtinstall download.
#
# Options:
#   --qt-dir <path>   Use an existing Qt install instead of auto-detecting/downloading
#                     (the folder containing bin/, lib/cmake/).
#   --no-run          Build but do not launch the editor.
set -euo pipefail

cd "$(dirname "$0")"
REPO_ROOT="$(pwd)"

# --- Pinned dependency versions (bump here to upgrade everyone consistently) ---
QT_VERSION="6.8.3"

QT_DIR_ARG=""
NO_RUN=0
while [ $# -gt 0 ]; do
    case "$1" in
        --qt-dir) QT_DIR_ARG="$2"; shift 2 ;;
        --no-run) NO_RUN=1; shift ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

# --- Per-OS aqt identifiers, install folder name, and base preset ---
case "$(uname -s)" in
    Darwin) AQT_HOST="mac";   QT_ARCH="clang_64";     QT_DIR_NAME="macos";  PRESET="macos" ;;
    Linux)  AQT_HOST="linux"; QT_ARCH="linux_gcc_64"; QT_DIR_NAME="gcc_64"; PRESET="linux" ;;
    *) echo "Unsupported OS '$(uname -s)'. Use setup.ps1 on Windows." >&2; exit 1 ;;
esac

require_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "!! Missing required tool '$1'. $2" >&2
        exit 1
    fi
}

# Look for an already-installed Qt in the usual places (the official Qt online
# installer drops it under ~/Qt/<ver>/<arch>). Prefer the pinned version, then
# the newest installed 6.x. Echoes the prefix path if found.
find_system_qt() {
    local version="$1"
    local roots="$HOME/Qt /opt/Qt"
    [ "$AQT_HOST" = "mac" ] && roots="$roots /Applications/Qt"
    # Exact pinned version first.
    for root in $roots; do
        if [ -d "$root/$version/$QT_DIR_NAME/lib/cmake" ]; then
            echo "$root/$version/$QT_DIR_NAME"; return 0
        fi
    done
    # Newest installed 6.x otherwise.
    for root in $roots; do
        [ -d "$root" ] || continue
        for v in $(ls -1 "$root" 2>/dev/null | grep '^6\.' | sort -Vr); do
            if [ -d "$root/$v/$QT_DIR_NAME/lib/cmake" ]; then
                echo "$root/$v/$QT_DIR_NAME"; return 0
            fi
        done
    done
    return 1
}

echo ">> Checking baseline tools..."
require_tool git    "Install git via your package manager."
require_tool cmake  "Install CMake >= 3.23 (brew install cmake / apt install cmake)."
require_tool ninja  "Install Ninja (brew install ninja / apt install ninja-build)."
require_tool python3 "Install Python 3.10+ with dev headers (brew install python / apt install python3-dev)."
if [ "$AQT_HOST" = "linux" ]; then
    command -v cc >/dev/null 2>&1 || require_tool g++ "Install a C++ compiler (apt install build-essential)."
fi

echo ">> Initializing submodules..."
git submodule update --init --recursive

# --- Resolve Qt: explicit dir > existing .qt download > installed Qt > aqt download ---
QT_DOWNLOAD_DIR="$REPO_ROOT/.qt"
QT_AUTO_PATH="$QT_DOWNLOAD_DIR/$QT_VERSION/$QT_DIR_NAME"

if [ -n "$QT_DIR_ARG" ]; then
    QT_PREFIX="$QT_DIR_ARG"
    echo ">> Using provided Qt: $QT_PREFIX"
elif [ -d "$QT_AUTO_PATH/lib/cmake" ]; then
    QT_PREFIX="$QT_AUTO_PATH"
    echo ">> Reusing previously downloaded Qt: $QT_PREFIX"
elif SYS_QT="$(find_system_qt "$QT_VERSION")"; then
    QT_PREFIX="$SYS_QT"
    echo ">> Using installed Qt: $QT_PREFIX"
else
    echo ">> No Qt found locally. Downloading Qt $QT_VERSION ($QT_ARCH) via aqtinstall (first run only)..."
    VENV="$REPO_ROOT/.venv-setup"
    [ -d "$VENV" ] || python3 -m venv "$VENV"
    "$VENV/bin/python" -m pip install --quiet --upgrade pip aqtinstall
    # Don't let a flaky download abort the whole script; handle it with guidance.
    "$VENV/bin/python" -m aqt install-qt "$AQT_HOST" desktop "$QT_VERSION" "$QT_ARCH" -O "$QT_DOWNLOAD_DIR" || true
    QT_PREFIX="$QT_AUTO_PATH"
    if [ ! -d "$QT_PREFIX/lib/cmake" ]; then
        echo ""                                                                                      >&2
        echo "!! Automatic Qt download failed (aqtinstall could not fetch Qt $QT_VERSION)."          >&2
        echo "   This can happen when aqt lags behind Qt's newest releases or a mirror is down."     >&2
        echo "   Do one of the following, then re-run this script:"                                  >&2
        echo "     1. Install Qt $QT_VERSION (Desktop) with the official Qt Online Installer:"       >&2
        echo "        https://www.qt.io/download-qt-installer"                                        >&2
        echo "        It will be auto-detected under ~/Qt/$QT_VERSION/$QT_DIR_NAME."                  >&2
        echo "     2. Or point at an existing Qt:  ./setup.sh --qt-dir /path/to/Qt/<ver>/$QT_DIR_NAME" >&2
        exit 1
    fi
fi

# --- Generate CMakeUserPresets.json (gitignored) so CLI + VS Code both find Qt ---
echo ">> Writing CMakeUserPresets.json -> $QT_PREFIX"
cat > "$REPO_ROOT/CMakeUserPresets.json" <<EOF
{
  "version": 6,
  "configurePresets": [
    {
      "name": "local",
      "displayName": "RockEngine (local Qt)",
      "inherits": "$PRESET",
      "cacheVariables": {
        "CMAKE_PREFIX_PATH": "$QT_PREFIX"
      }
    }
  ],
  "buildPresets": [
    { "name": "local", "configurePreset": "local" }
  ]
}
EOF

# --- Configure + build ---
echo ">> Configuring (preset: local)..."
cmake --preset local
echo ">> Building (preset: local)..."
cmake --build --preset local

LAUNCHER="$REPO_ROOT/build/local/bin/RockEngineLauncher"
echo ">> Done. Launcher: $LAUNCHER"
if [ "$NO_RUN" -eq 0 ] && [ -x "$LAUNCHER" ]; then
    export LD_LIBRARY_PATH="$QT_PREFIX/lib:${LD_LIBRARY_PATH:-}"
    export DYLD_LIBRARY_PATH="$QT_PREFIX/lib:${DYLD_LIBRARY_PATH:-}"
    echo ">> Launching..."
    "$LAUNCHER"
fi
