#!/usr/bin/env bash
# Thin cross-platform build wrapper for RockEngine.
#
# Run the bootstrap FIRST so Qt is present and CMakeUserPresets.json exists:
#   Windows:        ./setup.ps1
#   macOS / Linux:  ./setup.sh
# Those generate the 'local' CMake preset this script drives.
#
# Usage:
#   ./build.sh          configure + build + run
#   ./build.sh --all    wipe the build dir first, then configure + build + run
set -euo pipefail

ALL=0
for arg in "$@"; do
    case "$arg" in
        --all) ALL=1 ;;
        *) ;;
    esac
done

PRESET="local"
BUILD_DIR="build/${PRESET}"

if [ "$ALL" -eq 1 ]; then
    echo ">> Removing ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
fi

if ! cmake --preset "${PRESET}" 2>/dev/null; then
    echo "!! Could not configure preset '${PRESET}'." >&2
    echo "!! Run ./setup.sh (macOS/Linux) or ./setup.ps1 (Windows) first," >&2
    echo "!! or copy CMakeUserPresets.json.example and set your Qt path." >&2
    exit 1
fi

cmake --build --preset "${PRESET}"

# Resolve the launcher (extension differs per OS).
BIN="${BUILD_DIR}/bin/RockEngineLauncher"
if [ -x "${BIN}.exe" ]; then
    BIN="${BIN}.exe"
fi

# macOS/Linux: help the app find the Qt libs at runtime when Qt isn't system-wide.
if [ -n "${CMAKE_PREFIX_PATH:-}" ]; then
    export LD_LIBRARY_PATH="${CMAKE_PREFIX_PATH}/lib:${LD_LIBRARY_PATH:-}"
    export DYLD_LIBRARY_PATH="${CMAKE_PREFIX_PATH}/lib:${DYLD_LIBRARY_PATH:-}"
fi

echo ">> Running ${BIN}"
"${BIN}"
