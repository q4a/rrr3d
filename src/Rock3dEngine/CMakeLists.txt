project(Rock3dEngine)

add_definitions(-DNOMINMAX) # Use std::max()/std::min()

list(APPEND PROJECT_DIRS
    "header"
    "header/graph"
    "header/graph/Driver"
    "header/gui"
    "header/include"
    "header/include/graph"
    "header/px"
    "header/res"
    "source"
    "source/graph"
    "source/graph/Driver"
    "source/gui"
    "source/px"
    "source/res"
    )
add_dir("${PROJECT_DIRS}" "${PROJECT_NAME}")

list(REMOVE_ITEM ${PROJECT_NAME}_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/header/graph/GaussianBlur.h")
list(REMOVE_ITEM ${PROJECT_NAME}_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/source/graph/GaussianBlur.cpp")

add_library(${PROJECT_NAME} STATIC
    ${${PROJECT_NAME}_SOURCE}
    ${${PROJECT_NAME}_INLINE}
    ${${PROJECT_NAME}_HEADER}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    header
    ${CMAKE_SOURCE_DIR}/src/LexStd/header
    ${CMAKE_SOURCE_DIR}/src/MathLib/header
)

if (MSVC)
    # PhysX 2.8.4 and the DirectX SDK, from the prebuilt extern.7z
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/physx/include/Cooking
        ${CMAKE_SOURCE_DIR}/extern/physx/include/Foundation
        ${CMAKE_SOURCE_DIR}/extern/physx/include/Physics
        ${CMAKE_SOURCE_DIR}/extern/physx/include/PhysXLoader
        ${CMAKE_SOURCE_DIR}/extern/directx/include
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    LexStd
)

if (NOT WIN32)
    # Windows/COM substitutes plus the vendored d3d9.h
    target_link_libraries(${PROJECT_NAME} PRIVATE XPlatform MathLib)

    # PhysX 4.1, built by tools/setup-physx-macos.sh. PhysX 2.8.4 is closed
    # source with no 64-bit or macOS build, so there is nothing to shim to.
    set(PHYSX41 ${CMAKE_SOURCE_DIR}/extern/physx41)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${PHYSX41}/physx/include
        ${PHYSX41}/pxshared/include
    )
endif()
