#!/bin/bash
set -e

# Tell CMake where to find Qt's CMake files
export CMAKE_PREFIX_PATH="C:/Qt/6.10.0/mingw_64/lib/cmake"

# Add Qt's compiler and tools to PATH
export PATH="C:/Qt/Tools/mingw1310_64/bin:C:/Qt/6.10.0/mingw_64/bin:$PATH"

BUILD_DIR="build"

mkdir -p "$BUILD_DIR"

# Only configure if cache doesn’t exist
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "Configuring CMake..."
    cmake -S . -B "$BUILD_DIR" \
        -G "MinGW Makefiles" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe" \
        -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe"
fi

# Build using all available cores
cmake --build "$BUILD_DIR" -j$(nproc)

echo "Running RockEngine..."
./"$BUILD_DIR"/RockEngine hello world
