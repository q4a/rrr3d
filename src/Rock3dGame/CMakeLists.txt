project(Rock3dGame)

add_definitions(-DNOMINMAX) # Use std::max()/std::min()


list(APPEND PROJECT_DIRS
    "header"
    "header/edit"
    "header/game"
    "header/net"
    "header/snd"
    "header/video"
    "include"
    "source"
    "source/edit"
    "source/game"
    "source/net"
    "source/snd"
    "source/video"
    )
add_dir("${PROJECT_DIRS}" "${PROJECT_NAME}")

add_library(${PROJECT_NAME} SHARED
    ${${PROJECT_NAME}_SOURCE}
    ${${PROJECT_NAME}_INLINE}
    ${${PROJECT_NAME}_HEADER}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    header
    include
    ${CMAKE_SOURCE_DIR}/src/LexStd/header
    ${CMAKE_SOURCE_DIR}/src/MathLib/header
    ${CMAKE_SOURCE_DIR}/src/NetLib/include
    ${CMAKE_SOURCE_DIR}/src/Rock3dEngine/header
    ${CMAKE_SOURCE_DIR}/extern/ogg/include
    ${CMAKE_SOURCE_DIR}/extern/physx/include/Cooking
    ${CMAKE_SOURCE_DIR}/extern/physx/include/Foundation
    ${CMAKE_SOURCE_DIR}/extern/physx/include/Physics
    ${CMAKE_SOURCE_DIR}/extern/physx/include/PhysXLoader
    ${CMAKE_SOURCE_DIR}/extern/vorbis/include
    ${CMAKE_SOURCE_DIR}/extern/directx/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    LexStd
    MathLib
    NetLib
    Rock3dEngine
    libogg_static.lib
    libvorbis_static.lib
    libvorbisfile_static.lib
    "$<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/extern/tinyxml/lib/release/tinyxml_STL.lib>"
    "$<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/extern/tinyxml/lib/debug/tinyxmld_STL.lib>"
    PhysXLoader.lib
    PhysXCooking.lib
    dxerr.lib
    dxguid.lib
    d3dx9.lib
    d3d9.lib
    strmiids.lib
    winmm.lib
    X3daudio.lib
    xinput.lib
)

target_link_directories(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/extern/ogg/lib/release>"
    "$<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/extern/ogg/lib/debug>"
    "$<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/extern/vorbis/lib/release>"
    "$<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/extern/vorbis/lib/debug>"
    ${CMAKE_SOURCE_DIR}/extern/physx/lib
    ${CMAKE_SOURCE_DIR}/extern/directx/lib
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    -DROCK3DGAME_EXPORTS
)
