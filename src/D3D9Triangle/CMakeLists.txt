# A minimal D3D9 triangle straight onto the Metal backend.
#
# The native equivalent of d9mt's test/triangle.c, which is Win32-only. Nothing
# of the game is involved, so it answers one question on its own: does the
# backend draw at all? When the game renders nothing, this says whether the
# fault is in the backend or in how the engine drives it.
#
# Writes tri.tga on frame 100, because a CAMetalLayer's contents do not appear
# in screencapture.
project(D3D9Triangle)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE XPlatform D3D9Metal)

find_package(SDL3 REQUIRED CONFIG)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
