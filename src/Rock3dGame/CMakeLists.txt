project(Rock3dGame)

if(WIN32)
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
else()
    # Milestone 3 deliberately exposes unavailable subsystems instead of
    # returning fake resources. Full gameplay sources enter the target only
    # after their renderer/physics dependencies have real backends.
    set(_rrr3d_portable_game_sources
        source/stub/MusicCat.cpp
        source/stub/PortableGame.cpp
        include/InputActions.h
        include/MusicCat.h
        include/PortableGame.h
        include/PortableInput.h
        include/OriginalAudioSpec.h
        include/MainMenu2Spec.h
        include/Rock3dGame.h
        include/IWorld.h
        include/IView.h
    )
    if(RRR3D_BUILD_PORTABLE_MENU)
        list(APPEND _rrr3d_portable_game_sources
            source/stub/PortableMenu.cpp
            include/PortableMenu.h
        )
    endif()
    if(RRR3D_BUILD_ORIGINAL_MENU)
        list(APPEND _rrr3d_portable_game_sources
            source/stub/OriginalMainMenu.cpp
            include/OriginalMainMenu.h
        )
    endif()
    if(RRR3D_ENABLE_PHYSICS AND RRR3D_BUILD_ORIGINAL_MENU)
        list(APPEND _rrr3d_portable_game_sources
            source/stub/OriginalRace.cpp
            source/stub/OriginalRaceSession.cpp
            source/stub/OriginalProfile.cpp
            include/OriginalRace.h
            include/OriginalRaceSession.h
            include/OriginalProfile.h
        )
    elseif(RRR3D_ENABLE_PHYSICS)
        list(APPEND _rrr3d_portable_game_sources
            source/stub/PortableRace.cpp
            include/PortableRace.h
        )
    endif()
    add_library(${PROJECT_NAME} STATIC ${_rrr3d_portable_game_sources})
endif()

target_include_directories(${PROJECT_NAME}
    PUBLIC
        include
    PRIVATE
        header
)

target_link_libraries(${PROJECT_NAME} PUBLIC Rock3dEngine)

if(TARGET NetLib)
    target_link_libraries(${PROJECT_NAME} PRIVATE NetLib)
endif()

if(WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/NetLib/include
        ${CMAKE_SOURCE_DIR}/extern/ogg/include
        ${CMAKE_SOURCE_DIR}/extern/physx/include/Cooking
        ${CMAKE_SOURCE_DIR}/extern/physx/include/Foundation
        ${CMAKE_SOURCE_DIR}/extern/physx/include/Physics
        ${CMAKE_SOURCE_DIR}/extern/physx/include/PhysXLoader
        ${CMAKE_SOURCE_DIR}/extern/vorbis/include
        ${CMAKE_SOURCE_DIR}/extern/directx/include
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
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
else()
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        RRR3D_PORTABLE_GAME_STUBS=1
    )
    if(RRR3D_ENABLE_PHYSICS AND RRR3D_BUILD_ORIGINAL_MENU)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${RRR3D_TINYXML_TARGET})
    endif()
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE ROCK3DGAME_EXPORTS)
rrr3d_set_common_target_options(${PROJECT_NAME})
