# Behavioural checks for the vehicle model, offscreen and without the game.
#
# The third harness. D3D9Triangle and BridgeTriangle bracket the graphics
# stack; this one brackets the physics, for the same reason -- a 130k-line game
# is a poor place to ask whether a suspension spring is right, and the vehicle
# model is the one part of this port that cannot be judged by looking at it.
#
# Not a parity test against PhysX 2.8, which is not available to link: see the
# note at the top of main.cpp.
project(PhysicsHarness)

add_definitions(-DNOMINMAX) # Use std::max()/std::min()

add_executable(${PROJECT_NAME} main.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Rock3dEngine/header
    ${CMAKE_SOURCE_DIR}/src/LexStd/header
    ${CMAKE_SOURCE_DIR}/src/MathLib/header
)

target_link_libraries(${PROJECT_NAME} PRIVATE Rock3dEngine LexStd MathLib)

if (NOT WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE XPlatform)

    set(PHYSX41 ${CMAKE_SOURCE_DIR}/extern/physx41)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${PHYSX41}/physx/include
        ${PHYSX41}/pxshared/include
    )
endif()

add_test(NAME physics-harness COMMAND ${PROJECT_NAME})
