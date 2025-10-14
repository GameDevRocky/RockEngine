cmake_minimum_required(VERSION 3.16)
project(RockEngine VERSION 1.0 LANGUAGES CXX)

# Use C++17 (or higher)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable automatic MOC/UIC/RCC handling for Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt package (adjust to your Qt version)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Source files
set(SOURCES
    src/main.cpp
)

# Create the executable
add_executable(RockEngine ${SOURCES})

# Link Qt modules
target_link_libraries(RockEngine PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
