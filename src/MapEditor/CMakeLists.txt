project(MapEditor)

list(APPEND PROJECT_DIRS
    "."
    )
add_dir("${PROJECT_DIRS}" "${PROJECT_NAME}")

add_definitions(-D_AFXDLL)
set(CMAKE_MFC_FLAG 2)

add_executable(${PROJECT_NAME} WIN32
    ${${PROJECT_NAME}_SOURCE}
    ${${PROJECT_NAME}_INLINE}
    ${${PROJECT_NAME}_HEADER}
    "res/MapEditor.rc2"
    "MapEditor.reg"
    "MapEditor.rc"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/LexStd/header
    ${CMAKE_SOURCE_DIR}/src/MathLib/header
    ${CMAKE_SOURCE_DIR}/src/Rock3dGame/include
    ${CMAKE_SOURCE_DIR}/extern/glm/include
    ${CMAKE_SOURCE_DIR}/extern/directx/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    LexStd
    MathLib
    Rock3dGame
    ${CMAKE_SOURCE_DIR}/extern/directx/lib/d3dx9.lib
)
