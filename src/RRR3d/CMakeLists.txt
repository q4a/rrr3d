project(RRR3d)

add_definitions(-DNOMINMAX) # Use std::max()/std::min()

# One shell per platform, and they are alternatives rather than layers: the
# SDL3 one does not translate events into MSG structs, because MainWndProc only
# ever existed to undo Win32 delivering input as messages.
if (MSVC)
    set(SRC_FILES
        "RRR3d.cpp"
        "stdafx.cpp"
        "stdafx.h"
    )
else()
    set(SRC_FILES
        "sdl_shell.cpp"
        "stdafx.cpp"
        "stdafx.h"
    )
endif()

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
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)

    target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::SDL3)
endif()
