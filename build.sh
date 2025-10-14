#!/bin/bash
set -e  

export CMAKE_PREFIX_PATH="C:/Qt/6.10.0/mingw_64/lib/cmake"

export PATH="C:/Qt/6.10.0/mingw_64/bin:$PATH"

BUILD_DIR="build"

mkdir -p "$BUILD_DIR"

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "Configuring CMake..."
    cmake -S . -B "$BUILD_DIR" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
fi

cmake --build "$BUILD_DIR" -j$(nproc)

echo "Running RockEngine..."
./"$BUILD_DIR"/RockEngine
