project(RRR3d)

add_definitions(-DNOMINMAX) # Use std::max()/std::min()

set(SRC_FILES
    "RRR3d.cpp"
    "stdafx.cpp"
    "stdafx.h"
)

add_executable(${PROJECT_NAME} WIN32 ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/LexStd/header
    ${CMAKE_SOURCE_DIR}/src/MathLib/header
    ${CMAKE_SOURCE_DIR}/src/Rock3dGame/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    LexStd
    MathLib
    Rock3dGame
)

if (MSVC)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/directx/include
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/directx/lib/d3dx9.lib
    )
endif()

if (NOT WIN32)
    # Win32 substitutes, and the D3DX seam -- see
    # src/XPlatform/source/d3d9_stub.cpp.
    target_link_libraries(${PROJECT_NAME} PRIVATE XPlatform)
endif()

if (APPLE)
    # Direct3D 9 itself: DXVK's front-end over d9mt's Metal backend.
    target_link_libraries(${PROJECT_NAME} PRIVATE D3D9Metal)
endif()
