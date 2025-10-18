#!/bin/bash
set -e

# Qt CMake path
export CMAKE_PREFIX_PATH="C:/Qt/6.10.0/mingw_64/lib/cmake"

# Add Qt + MinGW to PATH
export PATH="C:/Qt/Tools/mingw1310_64/bin:C:/Qt/6.10.0/mingw_64/bin:$PATH"

BUILD_DIR="build"

mkdir -p "$BUILD_DIR"

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "Configuring CMake..."
    cmake -S . -B "$BUILD_DIR" \
        -G "MinGW Makefiles" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_COMPILER="C:/Qt/Tools/mingw1310_64/bin/gcc.exe" \
        -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe" \
        -DCMAKE_PREFIX_PATH="C:/Qt/6.10.0/mingw_64/lib/cmake"
fi


# Determine number of cores (default 4)
JOBS=$(nproc 2>/dev/null || echo 4)

# Build
cmake --build "$BUILD_DIR" -- -j$JOBS

# Run executable
"$BUILD_DIR/RockEngine.exe" hello world
