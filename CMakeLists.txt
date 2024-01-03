cmake_minimum_required(VERSION 3.16)

project(openxr-tutorial)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")

# option(TUTORIAL_BUILD_DOCUMENTATION "Build the tutorial documentation?" ON)
# option(TUTORIAL_BUILD_PROJECTS "Build the tutorial projects?" ON)

# Optional override runtime
set(XR_RUNTIME_JSON
    "$ENV{XR_RUNTIME_JSON}"
    CACHE PATH
        "Optional location of a specific OpenXR runtime configuration file."
)

# cmake_minimum_required(VERSION 3.22.1)
# set(PROJECT_NAME OpenXRTutorialChapter3)
# project("${PROJECT_NAME}") # Needed for Android

# Additional Directories for find_package() to search within.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# For FetchContent_Declare() and FetchContent_MakeAvailable()
include(FetchContent)

# openxr_loader - From github.com/KhronosGroup
set(BUILD_TESTS
  OFF
  CACHE INTERNAL "Build tests"
)
set(BUILD_API_LAYERS
  ON
  CACHE INTERNAL "Use OpenXR layers"
)
FetchContent_Declare(
  OpenXR
  URL_HASH MD5=81930f0ccecdca852906e1a22aee4a45
  URL https://github.com/KhronosGroup/OpenXR-SDK-Source/archive/refs/tags/release-1.0.28.zip
    SOURCE_DIR
    openxr
)
FetchContent_MakeAvailable(OpenXR)


# Files
set(SOURCES
  "main.cpp"
  "./Common/GraphicsAPI.cpp"
  "./Common/GraphicsAPI_OpenGL.cpp"
  "./Common/OpenXRDebugUtils.cpp")
set(HEADERS
  "./Common/DebugOutput.h"
  "./Common/GraphicsAPI.h"
  "./Common/GraphicsAPI_OpenGL.h"
  "./Common/HelperFunctions.h"
  "./Common/OpenXRDebugUtils.h"
  "./Common/OpenXRHelper.h"
  "./Common/steam/steam_api.h")
set(GLSL_SHADERS
  "./Shaders/VertexShader.glsl"
  "./Shaders/PixelShader.glsl")


add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

if(NOT "${XR_RUNTIME_JSON}" STREQUAL "")
  set_target_properties( ${PROJECT_NAME} PROPERTIES VS_DEBUGGER_ENVIRONMENT "XR_RUNTIME_JSON=${XR_RUNTIME_JSON}")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE 
  # In this repo
  ./Common/
  # From OpenXR repo
  "${openxr_SOURCE_DIR}/src/common"
  "${openxr_SOURCE_DIR}/external/include"
)
target_link_libraries(${PROJECT_NAME} openxr_loader)

# Wayland Specified
target_compile_definitions(${PROJECT_NAME} PUBLIC XR_TUTORIAL_USE_LINUX_WAYLAND)


# OpenGL
include(./cmake/gfxwrapper.cmake)
if(TARGET openxr-gfxwrapper)
  target_link_libraries(${PROJECT_NAME} openxr-gfxwrapper)
  target_compile_definitions(${PROJECT_NAME} PUBLIC XR_TUTORIAL_USE_OPENGL)
endif()

# OpenGL GLSL
set(SHADER_DEST "${CMAKE_CURRENT_BINARY_DIR}")
foreach(FILE ${GLSL_SHADERS})
  get_filename_component(FILE_WE ${FILE} NAME_WE)
  add_custom_command(
    OUTPUT "${SHADER_DEST}/${FILE_WE}.glsl"
    COMMAND 
      ${CMAKE_COMMAND} -E copy 
      "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"
      "${SHADER_DEST}/${FILE_WE}.glsl"
    COMMENT "GLSL ${FILE}"
    MAIN_DEPENDENCY "${FILE}"
    DEPEND "${FILE}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM
  )
  # Make our project depend on these files
  target_sources(${PROJECT_NAME} PRIVATE "${SHADER_DEST}/${FILE_WE}.glsl")
endforeach(FILE)


# Copy DLLs and subfolders to the build directory during the build process
add_custom_command(
  TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/bin  # Source directory with DLLs and subfolders
    $<TARGET_FILE_DIR:${PROJECT_NAME}>  # Destination directory
)

# Set the RPATH to include the directory with platform-specific DLLs
if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "$ORIGIN/linux64"  # Update with your directory name
  )
endif()
