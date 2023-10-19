cmake_minimum_required(VERSION 3.16)

project(openxr-tutorial)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")

option(TUTORIAL_BUILD_DOCUMENTATION "Build the tutorial documentation?" ON)
option(TUTORIAL_BUILD_PROJECTS "Build the tutorial projects?" ON)

# Optional override runtime
set(XR_RUNTIME_JSON
    "$ENV{XR_RUNTIME_JSON}"
    CACHE PATH
        "Optional location of a specific OpenXR runtime configuration file."
)

add_subdirectory(Chapter2)
add_subdirectory(Chapter3)