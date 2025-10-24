# GLM (header-only)
add_subdirectory(glm)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_UTILS OFF CACHE BOOL "" FORCE)

add_subdirectory(yaml)
add_subdirectory(box2d)
add_library(glad)
