cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

set(RRR3D_VERSION "1.3.1")
if(APPLE)
    project(rrr3d VERSION ${RRR3D_VERSION} LANGUAGES C CXX OBJCXX)
else()
    project(rrr3d VERSION ${RRR3D_VERSION} LANGUAGES C CXX)
endif()

option(RRR3D_BUILD_GAME "Build RRR3D game" ON)
option(RRR3D_BUILD_MAP_EDITOR "Build map editor" OFF)
option(RRR3D_BUILD_SDL_SHELL "Build the SDL3 window and input shell" OFF)
option(RRR3D_BUILD_DXVK_MOLTENVK_TEST
    "Build the isolated D3D9 -> DXVK Native -> MoltenVK feasibility test" OFF)
option(RRR3D_BUILD_BGFX_SCENE
    "Build the Milestone 5 static 3D scene with the portable renderer" OFF)
option(RRR3D_BUILD_PORTABLE_MENU
    "Build the superseded portable vertical-slice menu" OFF)
option(RRR3D_BUILD_ORIGINAL_MENU
    "Build the Milestone 6 original ResourceManager/MainMenu2 renderer slice" OFF)
option(RRR3D_BUILD_MACOS_BUNDLE
    "Package the active game port as an autonomous macOS application bundle" OFF)
option(RRR3D_ENABLE_RENDERER "Enable the game renderer" OFF)
option(RRR3D_ENABLE_NETWORK "Enable networking" OFF)
option(RRR3D_ENABLE_VIDEO "Enable video playback" OFF)
option(RRR3D_ENABLE_PHYSICS "Enable physics" OFF)
option(RRR3D_ENABLE_STEAM "Enable Steam integration" OFF)
option(RRR3D_ENABLE_GAMEPAD "Enable SDL3 gamepad input" OFF)
option(RRR3D_ENABLE_AUDIO "Enable SDL3 audio with Ogg/Vorbis decoding" OFF)
option(RRR3D_ENABLE_DXVK_NATIVE "Use DXVK Native renderer" OFF)
option(RRR3D_MACOS_HARDENED_RUNTIME
    "Enable the hardened runtime when signing a macOS bundle" OFF)

set(RRR3D_RENDERER_BACKEND "none" CACHE STRING
    "Portable renderer backend (none or bgfx)")
set_property(CACHE RRR3D_RENDERER_BACKEND PROPERTY STRINGS none bgfx)

set(RRR3D_DXVK_NATIVE_ROOT "" CACHE PATH
    "Installed prefix of the pinned, macOS-patched DXVK Native build")
set(RRR3D_MOLTENVK_ROOT "" CACHE PATH
    "Optional MoltenVK prefix containing include/ and lib/")
set(RRR3D_BGFX_ROOT "${CMAKE_SOURCE_DIR}/build/bgfx-macos" CACHE PATH
    "Root populated by scripts/build_bgfx_macos.sh")
set(RRR3D_MACOS_CODESIGN_IDENTITY "-" CACHE STRING
    "Code-signing identity for the macOS application bundle (- for ad-hoc)")

if(RRR3D_BUILD_MACOS_BUNDLE)
    if(NOT APPLE)
        message(FATAL_ERROR "RRR3D_BUILD_MACOS_BUNDLE is available only on macOS")
    endif()
    if(RRR3D_BUILD_PORTABLE_MENU OR
       NOT RRR3D_BUILD_ORIGINAL_MENU OR
       NOT RRR3D_ENABLE_RENDERER OR
       NOT RRR3D_ENABLE_GAMEPAD OR
       NOT RRR3D_ENABLE_AUDIO OR
       NOT RRR3D_ENABLE_VIDEO OR
       NOT RRR3D_ENABLE_PHYSICS)
        message(FATAL_ERROR
            "The macOS bundle requires the active source-driven runtime: "
            "original menu/resources, renderer, gamepad, audio, video, and physics; "
            "the superseded portable vertical slice must be disabled")
    endif()
    if(RRR3D_MACOS_CODESIGN_IDENTITY STREQUAL "")
        message(FATAL_ERROR
            "RRR3D_MACOS_CODESIGN_IDENTITY must name an identity or '-' for ad-hoc signing")
    endif()
elseif(RRR3D_MACOS_HARDENED_RUNTIME)
    message(FATAL_ERROR
        "RRR3D_MACOS_HARDENED_RUNTIME requires RRR3D_BUILD_MACOS_BUNDLE=ON")
endif()

if(RRR3D_BUILD_BGFX_SCENE AND
   (RRR3D_BUILD_PORTABLE_MENU OR RRR3D_BUILD_ORIGINAL_MENU))
    message(FATAL_ERROR
        "The Milestone 5 scene and menu entry points are separate "
        "milestone entry points and cannot be enabled together")
endif()

if(RRR3D_BUILD_PORTABLE_MENU AND RRR3D_BUILD_ORIGINAL_MENU)
    message(FATAL_ERROR
        "The original Milestone 6 menu cannot be combined with the "
        "superseded portable vertical slice")
endif()

if(RRR3D_BUILD_PORTABLE_MENU AND NOT RRR3D_BUILD_GAME)
    message(FATAL_ERROR
        "RRR3D_BUILD_PORTABLE_MENU requires RRR3D_BUILD_GAME=ON")
endif()

if(RRR3D_BUILD_ORIGINAL_MENU AND NOT RRR3D_BUILD_GAME)
    message(FATAL_ERROR
        "RRR3D_BUILD_ORIGINAL_MENU requires RRR3D_BUILD_GAME=ON")
endif()

if(RRR3D_ENABLE_PHYSICS AND NOT WIN32 AND
   NOT RRR3D_BUILD_PORTABLE_MENU AND NOT RRR3D_BUILD_ORIGINAL_MENU)
    message(FATAL_ERROR
        "The Milestone 9 physics race requires a menu entry point")
endif()

if(RRR3D_ENABLE_PHYSICS AND NOT WIN32 AND NOT RRR3D_ENABLE_GAMEPAD)
    message(FATAL_ERROR
        "The portable Milestone 9 physics race requires "
        "RRR3D_ENABLE_GAMEPAD=ON for the SDL3 action layer")
endif()

if(RRR3D_BUILD_BGFX_SCENE OR RRR3D_BUILD_PORTABLE_MENU OR
   RRR3D_BUILD_ORIGINAL_MENU)
    if(NOT RRR3D_ENABLE_RENDERER OR
       NOT RRR3D_RENDERER_BACKEND STREQUAL "bgfx")
        message(FATAL_ERROR
            "The bgfx scene/menu requires RRR3D_ENABLE_RENDERER=ON and "
            "RRR3D_RENDERER_BACKEND=bgfx")
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS architectures")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_MACOSX_RPATH ON)
    add_compile_definitions(RRR3D_MACOS=1)
elseif(WIN32)
    add_compile_definitions(RRR3D_WINDOWS=1)
else()
    add_compile_definitions(RRR3D_LINUX=1)
endif()

include(${PROJECT_SOURCE_DIR}/cmake/utils.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/dependencies.cmake)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug)

add_subdirectory(src)
