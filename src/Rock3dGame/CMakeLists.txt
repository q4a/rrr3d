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

if (NOT MSVC)
    # The three files that actually build a DirectShow filter graph. They need
    # the real dshow.h plus Vmr9.h and Evr.h -- XPlatform's dshow.h deliberately
    # goes only as deep as the game's *headers* do, which is pointer members.
    #
    # Excluded rather than stubbed: the game tolerates video being absent
    # (World::IsVideoMode() treats a missing graph as no cutscene), so this is
    # the one subsystem where not linking is a state it can really run in.
    # Phase 11 puts them back. Nothing else changes -- World.h keeps its
    # video::Player member and World.cpp keeps its EC_* handler.
    list(REMOVE_ITEM ${PROJECT_NAME}_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/source/video/video.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/video/playback.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/video/VideoPlayer.cpp"
    )
else()
    # ...and its replacement is excluded on Windows, where the real three build.
    list(REMOVE_ITEM ${PROJECT_NAME}_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/source/video/video_stub.cpp"
    )
endif()

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
        dxerr.lib
        dxguid.lib
        d3dx9.lib
        d3d9.lib
        strmiids.lib
        winmm.lib
        X3daudio.lib
        xinput.lib
    )

    # The real SDK, and only when the shim is off -- otherwise the process gets
    # both implementations of the same API and whichever the linker picks first
    # wins silently. Rock3dEngine already links PhysX28 when the shim is on.
    if (NOT RRR3D_PHYSX28_SHIM)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            PhysXLoader.lib
            PhysXCooking.lib
        )
    endif()

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

    if (APPLE)
        # Direct3D 9 itself: DXVK's D3D9 front-end over d9mt's Metal backend.
        #
        # Here rather than in the executable because this is where the calls
        # are: D3D9RenderDriver is compiled into Rock3dEngine, which is a static
        # archive absorbed into this shared library. D3D9Metal is also a static
        # archive, so linking it here gives the process exactly one copy of
        # DXVK's device and its state.
        target_link_libraries(${PROJECT_NAME} PRIVATE D3D9Metal)
    endif()

    # Deliberately nothing here for XAudio2/X3DAudio, DirectShow or XInput.
    # Each arrives with the phase that ports it -- 11 -- and until then the
    # undefined symbols at link time are the list of what is left, which is
    # worth more than a stub that links and does nothing.
endif()

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    -DROCK3DGAME_EXPORTS
)
