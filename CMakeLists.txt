cmake_minimum_required(VERSION 3.21 FATAL_ERROR)
cmake_policy(VERSION 3.21)

set(RRR3D_VERSION "1.3.1")
# C as well as C++: the D3DX math implementation is Wine's, and it is C.
project(rrr3d VERSION ${RRR3D_VERSION} LANGUAGES C CXX)

# Objective-C and Objective-C++, for the Metal backend, and Apple-only because
# nothing else can compile them. Enabled here rather than in the project() line
# so the Windows build never asks for a compiler it does not have.
#
# Both, not one: MetalBridge is Objective-C++, but d9mt's d9mtmetal.m must be
# built as plain Objective-C -- it relies on void* converting implicitly, which
# C++ does not allow.
if (APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# No compiler extensions. The point of pinning C++17 here is to stop depending
# on what one compiler happens to allow.
set(CMAKE_CXX_EXTENSIONS OFF)

include(${PROJECT_SOURCE_DIR}/cmake/utils.cmake)

### Output paths
# The game resolves its assets relative to the executable and they live under
# bin/, so the binaries have to land there rather than beside their objects.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release")
# Single-config generators -- which is what both presets use -- ignore the
# per-config variables above, so place them by build type as well.
if(CMAKE_BUILD_TYPE)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

add_subdirectory(src)
