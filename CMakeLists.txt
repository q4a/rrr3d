cmake_minimum_required(VERSION 3.12 FATAL_ERROR)
cmake_policy(VERSION 3.12)

set(RRR3D_VERSION "1.3.1")
project(rrr3d VERSION ${RRR3D_VERSION} LANGUAGES CXX)

include(${PROJECT_SOURCE_DIR}/cmake/utils.cmake)

add_subdirectory(src)
