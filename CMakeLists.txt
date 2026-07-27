cmake_minimum_required(VERSION 3.12 FATAL_ERROR)
cmake_policy(VERSION 3.12)

set(RRR3D_VERSION "1.3.1")
# C is needed for the vendored Wine d3dx9 math implementation in MathLib.
project(rrr3d VERSION ${RRR3D_VERSION} LANGUAGES C CXX)

include(${PROJECT_SOURCE_DIR}/cmake/utils.cmake)

### Language level
# std::filesystem (used to open streams from wide paths portably) needs C++17.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

### Vendored Wine d3dx9 math
# It implements only what this project calls. The matrix stack needs COM
# machinery we do not otherwise require, and the spherical-harmonics cube map
# needs Wine's texture and pixel-format code. Neither is used here, so both are
# compiled out. Project-wide because several targets include d3dx9math.h
# without linking MathLib. See src/MathLib/header/d3d/d3dx9_compat.h.
add_compile_definitions(
    D3DX9_NO_MATRIX_STACK
    D3DX9_NO_SH_CUBEMAP
)

### PhysX requires exactly one of NDEBUG / _DEBUG to be defined. CMake's
### Release config defines NDEBUG, but its Debug config defines neither.
add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>)

### Set up output paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug")

add_subdirectory(src)
