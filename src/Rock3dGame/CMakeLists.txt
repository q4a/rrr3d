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
        source/stub/OriginalAudioSpec.cpp
        source/stub/OriginalControlBindings.cpp
        source/stub/OriginalGameData.cpp
        source/stub/PortableGame.cpp
        include/InputActions.h
        include/MusicCat.h
        include/PortableGame.h
        include/PortableInput.h
        include/OriginalAudioSpec.h
        include/OriginalControlBindings.h
        include/OriginalGameData.h
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
            source/stub/OriginalGameObject.cpp
            source/stub/OriginalCameraManager.cpp
            source/stub/OriginalAchievmentModel.cpp
            source/stub/OriginalGameCar.cpp
            source/stub/OriginalLogic.cpp
            source/stub/OriginalMainMenu.cpp
            source/stub/OriginalMap.cpp
            source/stub/OriginalMapObj.cpp
            source/stub/OriginalAICar.cpp
            source/stub/OriginalHumanPlayer.cpp
            source/stub/OriginalPlayer.cpp
            source/stub/OriginalRockCar.cpp
            source/stub/OriginalRaceLifecycle.cpp
            source/stub/OriginalSlot.cpp
            source/stub/OriginalTrace.cpp
            source/stub/OriginalTournament.cpp
            source/stub/OriginalUserChat.cpp
            source/stub/OriginalWeapon.cpp
            include/OriginalGameObject.h
            include/OriginalCameraManager.h
            include/OriginalAchievmentModel.h
            include/OriginalGameCar.h
            include/OriginalLogic.h
            include/OriginalMainMenu.h
            include/OriginalMap.h
            include/OriginalMapObj.h
            include/OriginalAICar.h
            include/OriginalHumanPlayer.h
            include/OriginalPlayer.h
            include/OriginalRockCar.h
            include/OriginalRaceLifecycle.h
            include/OriginalSlot.h
            include/OriginalTrace.h
            include/OriginalTournament.h
            include/OriginalUserChat.h
            include/OriginalWeapon.h
        )
    endif()
    if(RRR3D_ENABLE_PHYSICS AND RRR3D_BUILD_ORIGINAL_MENU)
        list(APPEND _rrr3d_portable_game_sources
            source/stub/OriginalRace.cpp
            source/stub/OriginalRaceSession.cpp
            source/stub/OriginalProfile.cpp
            source/stub/OriginalGarage.cpp
            include/OriginalRace.h
            include/OriginalRaceSession.h
            include/OriginalProfile.h
            include/OriginalGarage.h
        )
    elseif(RRR3D_ENABLE_PHYSICS)
        list(APPEND _rrr3d_portable_game_sources
            source/stub/PortableRace.cpp
            include/PortableRace.h
        )
    endif()
    if(RRR3D_ENABLE_NETWORK)
        list(APPEND _rrr3d_portable_game_sources
            source/stub/OriginalNetwork.cpp
            source/stub/OriginalNetworkModels.cpp
            include/OriginalNetwork.h
            include/OriginalNetworkModels.h
        )
    endif()
    add_library(${PROJECT_NAME} STATIC ${_rrr3d_portable_game_sources})
endif()

if(NOT WIN32 AND RRR3D_BUILD_ORIGINAL_MENU)
    add_executable(rrr3d_original_achievment_smoke
        tests/OriginalAchievmentSmoke.cpp
    )
    target_link_libraries(rrr3d_original_achievment_smoke PRIVATE
        ${PROJECT_NAME}
    )
    rrr3d_set_common_target_options(rrr3d_original_achievment_smoke)
    add_test(
        NAME rrr3d_original_achievment_smoke
        COMMAND rrr3d_original_achievment_smoke
    )

    add_executable(rrr3d_original_race_lifecycle_smoke
        tests/OriginalRaceLifecycleSmoke.cpp
    )
    target_link_libraries(rrr3d_original_race_lifecycle_smoke PRIVATE
        ${PROJECT_NAME}
    )
    rrr3d_set_common_target_options(
        rrr3d_original_race_lifecycle_smoke)
    add_test(
        NAME rrr3d_original_race_lifecycle_smoke
        COMMAND rrr3d_original_race_lifecycle_smoke
    )

    add_executable(rrr3d_original_user_chat_smoke
        tests/OriginalUserChatSmoke.cpp
    )
    target_link_libraries(rrr3d_original_user_chat_smoke PRIVATE
        ${PROJECT_NAME}
    )
    rrr3d_set_common_target_options(rrr3d_original_user_chat_smoke)
    add_test(
        NAME rrr3d_original_user_chat_smoke
        COMMAND rrr3d_original_user_chat_smoke
    )

    add_executable(rrr3d_portable_capabilities_smoke
        tests/PortableCapabilitiesSmoke.cpp
    )
    target_link_libraries(rrr3d_portable_capabilities_smoke PRIVATE
        ${PROJECT_NAME}
    )
    rrr3d_set_common_target_options(rrr3d_portable_capabilities_smoke)
    add_test(
        NAME rrr3d_portable_capabilities_smoke
        COMMAND rrr3d_portable_capabilities_smoke
    )

    add_executable(rrr3d_original_tournament_smoke
        tests/OriginalTournamentSmoke.cpp
    )
    target_link_libraries(rrr3d_original_tournament_smoke PRIVATE
        ${PROJECT_NAME}
    )
    rrr3d_set_common_target_options(rrr3d_original_tournament_smoke)
    add_test(
        NAME rrr3d_original_tournament_smoke
        COMMAND rrr3d_original_tournament_smoke
    )

    add_executable(rrr3d_original_trace_smoke
        tests/OriginalTraceSmoke.cpp
    )
    target_link_libraries(rrr3d_original_trace_smoke PRIVATE
        ${PROJECT_NAME}
    )
    rrr3d_set_common_target_options(rrr3d_original_trace_smoke)
    add_test(
        NAME rrr3d_original_trace_smoke
        COMMAND rrr3d_original_trace_smoke
    )

    add_executable(rrr3d_original_player_smoke
        tests/OriginalPlayerSmoke.cpp
    )
    target_link_libraries(rrr3d_original_player_smoke PRIVATE
        ${PROJECT_NAME}
    )
    rrr3d_set_common_target_options(rrr3d_original_player_smoke)
    add_test(
        NAME rrr3d_original_player_smoke
        COMMAND rrr3d_original_player_smoke
    )

    add_executable(rrr3d_original_game_object_smoke
        tests/OriginalGameObjectSmoke.cpp
    )
    target_link_libraries(rrr3d_original_game_object_smoke PRIVATE
        ${PROJECT_NAME}
    )
    rrr3d_set_common_target_options(
        rrr3d_original_game_object_smoke)
    add_test(
        NAME rrr3d_original_game_object_smoke
        COMMAND rrr3d_original_game_object_smoke
    )

    add_executable(rrr3d_original_map_obj_smoke
        tests/OriginalMapObjSmoke.cpp
    )
    target_link_libraries(rrr3d_original_map_obj_smoke PRIVATE
        ${PROJECT_NAME}
    )
    rrr3d_set_common_target_options(rrr3d_original_map_obj_smoke)
    add_test(
        NAME rrr3d_original_map_obj_smoke
        COMMAND rrr3d_original_map_obj_smoke
    )

    add_executable(rrr3d_original_map_smoke
        tests/OriginalMapSmoke.cpp
    )
    target_link_libraries(rrr3d_original_map_smoke PRIVATE
        ${PROJECT_NAME}
    )
    rrr3d_set_common_target_options(rrr3d_original_map_smoke)
    add_test(
        NAME rrr3d_original_map_smoke
        COMMAND rrr3d_original_map_smoke
    )

    add_executable(rrr3d_original_game_car_smoke
        tests/OriginalGameCarSmoke.cpp
    )
    target_link_libraries(rrr3d_original_game_car_smoke PRIVATE
        ${PROJECT_NAME}
    )
    rrr3d_set_common_target_options(
        rrr3d_original_game_car_smoke)
    add_test(
        NAME rrr3d_original_game_car_smoke
        COMMAND rrr3d_original_game_car_smoke
    )

    add_executable(rrr3d_original_ai_car_smoke
        tests/OriginalAICarSmoke.cpp
    )
    target_link_libraries(rrr3d_original_ai_car_smoke PRIVATE
        ${PROJECT_NAME}
    )
    rrr3d_set_common_target_options(rrr3d_original_ai_car_smoke)
    add_test(
        NAME rrr3d_original_ai_car_smoke
        COMMAND rrr3d_original_ai_car_smoke
    )

    add_executable(rrr3d_original_camera_manager_smoke
        tests/OriginalCameraManagerSmoke.cpp
    )
    target_link_libraries(rrr3d_original_camera_manager_smoke PRIVATE
        ${PROJECT_NAME}
    )
    rrr3d_set_common_target_options(
        rrr3d_original_camera_manager_smoke)
    add_test(
        NAME rrr3d_original_camera_manager_smoke
        COMMAND rrr3d_original_camera_manager_smoke
    )

    add_executable(rrr3d_original_weapon_smoke
        tests/OriginalWeaponSmoke.cpp
    )
    target_link_libraries(rrr3d_original_weapon_smoke PRIVATE
        ${PROJECT_NAME}
    )
    rrr3d_set_common_target_options(rrr3d_original_weapon_smoke)
    add_test(
        NAME rrr3d_original_weapon_smoke
        COMMAND rrr3d_original_weapon_smoke
    )

    add_executable(rrr3d_original_slot_smoke
        tests/OriginalSlotSmoke.cpp
    )
    target_link_libraries(rrr3d_original_slot_smoke PRIVATE
        ${PROJECT_NAME}
    )
    rrr3d_set_common_target_options(rrr3d_original_slot_smoke)
    add_test(
        NAME rrr3d_original_slot_smoke
        COMMAND rrr3d_original_slot_smoke
    )

    add_executable(rrr3d_original_human_player_smoke
        tests/OriginalHumanPlayerSmoke.cpp
    )
    target_link_libraries(rrr3d_original_human_player_smoke PRIVATE
        ${PROJECT_NAME}
    )
    rrr3d_set_common_target_options(
        rrr3d_original_human_player_smoke)
    add_test(
        NAME rrr3d_original_human_player_smoke
        COMMAND rrr3d_original_human_player_smoke
    )
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

if(NOT WIN32 AND RRR3D_ENABLE_NETWORK)
    target_compile_definitions(${PROJECT_NAME} PUBLIC RRR3D_NETWORK=1)
    add_executable(rrr3d_original_network_session_smoke
        tests/OriginalNetworkSessionSmoke.cpp
    )
    target_link_libraries(rrr3d_original_network_session_smoke PRIVATE
        ${PROJECT_NAME}
    )
    rrr3d_set_common_target_options(rrr3d_original_network_session_smoke)
    add_test(
        NAME rrr3d_original_network_session_smoke
        COMMAND rrr3d_original_network_session_smoke
    )
    add_executable(rrr3d_original_network_models_smoke
        tests/OriginalNetworkModelsSmoke.cpp
    )
    target_include_directories(rrr3d_original_network_models_smoke PRIVATE
        ${CMAKE_SOURCE_DIR}/src/NetLib/header
    )
    target_link_libraries(rrr3d_original_network_models_smoke PRIVATE
        ${PROJECT_NAME}
        NetLib
        ${RRR3D_BOOST_TARGET}
    )
    rrr3d_set_common_target_options(rrr3d_original_network_models_smoke)
    add_test(
        NAME rrr3d_original_network_models_smoke
        COMMAND rrr3d_original_network_models_smoke
    )
endif()

if(NOT WIN32 AND RRR3D_ENABLE_AUDIO)
    target_compile_definitions(${PROJECT_NAME} PUBLIC RRR3D_AUDIO=1)
endif()

if(NOT WIN32 AND RRR3D_ENABLE_VIDEO)
    target_compile_definitions(${PROJECT_NAME} PUBLIC RRR3D_VIDEO=1)
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
        # OriginalRace data and the portable commentator share the serialized
        # TinyXML schema, so consumers need the TinyXML 1 headers as well as
        # the static implementation.
        target_link_libraries(${PROJECT_NAME} PUBLIC ${RRR3D_TINYXML_TARGET})
    endif()
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE ROCK3DGAME_EXPORTS)
rrr3d_set_common_target_options(${PROJECT_NAME})
