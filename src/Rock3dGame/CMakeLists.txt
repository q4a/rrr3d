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
    ${RRR3D_PHYSX_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    LexStd
    MathLib
    NetLib
    Rock3dEngine
)

if (MSVC)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/ogg/include
        ${CMAKE_SOURCE_DIR}/extern/vorbis/include
        ${CMAKE_SOURCE_DIR}/extern/directx/include
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        libogg_static.lib
        libvorbis_static.lib
        libvorbisfile_static.lib
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
else()
    # Ogg and Vorbis are ordinary portable C libraries and are the same ones
    # either way; only how they are found differs.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(OGG REQUIRED IMPORTED_TARGET ogg)
    pkg_check_modules(VORBIS REQUIRED IMPORTED_TARGET vorbis vorbisfile)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        PkgConfig::OGG
        PkgConfig::VORBIS
    )

    # Deliberately nothing here for D3D9, D3DX, XAudio2/X3DAudio, DirectShow or
    # XInput. Each arrives with the phase that ports it -- 7, 8 and 11 -- and
    # until then the undefined symbols at link time are the list of what is
    # left, which is worth more than a stub that links and does nothing.
endif()

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    -DROCK3DGAME_EXPORTS
)
