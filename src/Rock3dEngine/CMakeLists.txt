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
    # IOverlay.h only: the one game-side interface the engine names, so a UI can
    # be drawn inside its frame. No link dependency comes with it.
    ${CMAKE_SOURCE_DIR}/src/Rock3dGame/include
    header
    ${CMAKE_SOURCE_DIR}/src/LexStd/header
    ${CMAKE_SOURCE_DIR}/src/MathLib/header
    ${RRR3D_PHYSX_INCLUDE_DIRS}
)

if (MSVC)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/directx/include
    )
else()
    target_link_libraries(${PROJECT_NAME} PUBLIC XPlatform)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    LexStd
    MathLib
)

# PUBLIC, because px/Physx.h is part of Rock3dEngine's own interface: it reaches
# Rock3dGame through stdafx.h, so anything linking the engine needs the Nx
# symbols too.
if (RRR3D_PHYSX28_SHIM)
    target_link_libraries(${PROJECT_NAME} PUBLIC PhysX28)
endif()
