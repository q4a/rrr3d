cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)
cmake_policy(VERSION 3.12)

set(RRR3D_VERSION "1.3.1")
project(rrr3d VERSION ${RRR3D_VERSION} LANGUAGES CXX)

include(${PROJECT_SOURCE_DIR}/cmake/utils.cmake)

### Set up output paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug)

if (MSVC)
    # DirectX SDK libs
    include_directories("${CMAKE_SOURCE_DIR}/extern/directx/include")
else()
    # Non Windows target specifics

    include(ExternalProject)

    # DXVK-native wrapper lib retrieval and building
    ExternalProject_Add(dxvk-native
            GIT_REPOSITORY    https://github.com/Joshua-Ashton/dxvk-native
            GIT_TAG           origin/master
            GIT_SHALLOW       ON
            BUILD_ALWAYS      OFF
            CONFIGURE_COMMAND meson ../dxvk-native
            BUILD_COMMAND     ninja
            INSTALL_COMMAND   ""
    )
    ExternalProject_Get_property(dxvk-native SOURCE_DIR BINARY_DIR)
    set(DXVK_NATIVE_INCLUDE_DIRS
            "${SOURCE_DIR}/include/native/directx"
            "${SOURCE_DIR}/include/native/windows"
    )
    set(DXVK_NATIVE_D3D9_LIB ${BINARY_DIR}/src/d3d9/libdxvk_d3d9.so)
    include_directories(
        "${CMAKE_SOURCE_DIR}/src/Platform"
        "${DXVK_NATIVE_INCLUDE_DIRS}"
    )
    ADD_CUSTOM_TARGET(dependencies ALL DEPENDS dxvk-native)
endif()

add_subdirectory(src)
