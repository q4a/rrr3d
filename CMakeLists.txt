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
elseif (LINUX)
    option(USE_OLD_DXVK "Use old DXVK Native for native D3D11 API" OFF)
    if (USE_OLD_DXVK)
        message(STATUS "Using DXVK Native for D3D9 API")
        set(DXVK_TYPE "native")
        set(DXVK_GIT_REPOSITORY "https://github.com/q4a/dxvk-native")
        set(DXVK_GIT_TAG "master")
        set(DXVK_EXTRA_PARAM "")
    else()
        message(STATUS "Using upstream DXVK for D3D9 API")
        set(DXVK_TYPE "upstream")
        set(DXVK_GIT_REPOSITORY "https://github.com/doitsujin/dxvk")
        set(DXVK_GIT_TAG "v2.7.1")
        set(DXVK_EXTRA_PARAM "-Denable_d3d8=false")
    endif()
    include(ExternalProject)
    set(DXVK_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/dxvk-src-${DXVK_TYPE}")
    set(DXVK_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/dxvk-bin-${DXVK_TYPE}")
    message(STATUS "DXVK_SOURCE_DIR=${DXVK_SOURCE_DIR}")
    message(STATUS "DXVK_BINARY_DIR=${DXVK_BINARY_DIR}")
    set(DIRECTX_INCLUDE_DIRS
        "${DXVK_SOURCE_DIR}/include/native/directx"
        "${DXVK_SOURCE_DIR}/include/native/windows"
    )
    set(DIRECTX_LIBS
        "${DXVK_BINARY_DIR}/src/d3d9/libdxvk_d3d9.so"
    )
    ExternalProject_Add(dxvk
        GIT_REPOSITORY    ${DXVK_GIT_REPOSITORY}
        GIT_TAG           ${DXVK_GIT_TAG}
        GIT_SHALLOW       OFF
        BUILD_ALWAYS      OFF
        SOURCE_DIR        ${DXVK_SOURCE_DIR}
        BINARY_DIR        ${DXVK_BINARY_DIR}
        CONFIGURE_HANDLED_BY_BUILD ON
        CONFIGURE_COMMAND meson setup ../dxvk-src-${DXVK_TYPE} --buildtype=release ${DXVK_EXTRA_PARAM} -Denable_d3d9=true -Denable_d3d10=false -Denable_d3d11=false -Denable_dxgi=false
        BUILD_COMMAND     ninja
        INSTALL_COMMAND   ""
        BUILD_BYPRODUCTS  ${DIRECTX_LIBS}
    )
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

# ...except the sanitizer build, which needs its own.
#
# It is a Debug build too, so without this it writes into bin/Debug alongside
# the ordinary one -- and since the two trees keep separate object files but
# share that destination, whichever built last silently owns the .dylibs there.
# A non-instrumented executable then loads an instrumented library and aborts in
# libc++ container annotations, which reads as a bug in whatever function
# happened to be running rather than as a build accident. Measured the hard way.
# The per-config variables have to go too: this is a Debug build, so the
# CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG set above would otherwise still win for
# executables, and only the libraries would move -- which splits the build in
# half and is worse than not moving it at all.
if(CMAKE_CXX_FLAGS MATCHES "sanitize=address")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/Asan")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

add_subdirectory(src)
