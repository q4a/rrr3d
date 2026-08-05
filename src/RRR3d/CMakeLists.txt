project(RRR3d)

if(WIN32)
    set(SRC_FILES
        RRR3d.cpp
        stdafx.cpp
        stdafx.h
    )
    add_executable(${PROJECT_NAME} WIN32 ${SRC_FILES})

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/Rock3dGame/include
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        LexStd
        MathLib
        Rock3dGame
        ${CMAKE_SOURCE_DIR}/extern/directx/lib/d3dx9.lib
    )
elseif(RRR3D_BUILD_PORTABLE_MENU OR RRR3D_BUILD_ORIGINAL_MENU OR
       RRR3D_BUILD_BGFX_SCENE)
    set(_rrr3d_shader_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    if(RRR3D_BUILD_BGFX_SCENE)
        set(_rrr3d_vertex_shader_source
            "${_rrr3d_shader_source_dir}/vs_legacy_mesh.sc")
        set(_rrr3d_fragment_shader_source
            "${_rrr3d_shader_source_dir}/fs_legacy_mesh.sc")
        set(_rrr3d_varying_definition
            "${_rrr3d_shader_source_dir}/varying_legacy_mesh.def.sc")
    else()
        set(_rrr3d_vertex_shader_source
            "${_rrr3d_shader_source_dir}/vs_static_scene.sc")
        set(_rrr3d_fragment_shader_source
            "${_rrr3d_shader_source_dir}/fs_static_scene.sc")
        set(_rrr3d_varying_definition
            "${_rrr3d_shader_source_dir}/varying.def.sc")
    endif()
    set(_rrr3d_shader_output_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(_rrr3d_vertex_shader
        "${_rrr3d_shader_output_dir}/rrr3d_vs_static_scene.bin.h")
    set(_rrr3d_fragment_shader
        "${_rrr3d_shader_output_dir}/rrr3d_fs_static_scene.bin.h")

    add_custom_command(
        OUTPUT "${_rrr3d_vertex_shader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${_rrr3d_shader_output_dir}"
        COMMAND "${RRR3D_BGFX_SHADERC}"
            --type vertex --platform osx --profile metal -O 3
            --varyingdef "${_rrr3d_varying_definition}"
            -i "${RRR3D_BGFX_SOURCE_DIR}/src"
            -f "${_rrr3d_vertex_shader_source}"
            -o "${_rrr3d_vertex_shader}"
            --bin2c rrr3d_vs_static_scene
        DEPENDS
            "${_rrr3d_vertex_shader_source}"
            "${_rrr3d_varying_definition}"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${_rrr3d_fragment_shader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${_rrr3d_shader_output_dir}"
        COMMAND "${RRR3D_BGFX_SHADERC}"
            --type fragment --platform osx --profile metal -O 3
            --varyingdef "${_rrr3d_varying_definition}"
            -i "${RRR3D_BGFX_SOURCE_DIR}/src"
            -f "${_rrr3d_fragment_shader_source}"
            -o "${_rrr3d_fragment_shader}"
            --bin2c rrr3d_fs_static_scene
        DEPENDS
            "${_rrr3d_fragment_shader_source}"
            "${_rrr3d_varying_definition}"
        VERBATIM
    )

    if(RRR3D_BUILD_ORIGINAL_MENU AND RRR3D_ENABLE_PHYSICS)
        set(_rrr3d_race_vertex_shader
            "${_rrr3d_shader_output_dir}/rrr3d_vs_original_race.bin.h")
        set(_rrr3d_race_fragment_shader
            "${_rrr3d_shader_output_dir}/rrr3d_fs_original_race.bin.h")
        add_custom_command(
            OUTPUT "${_rrr3d_race_vertex_shader}"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "${_rrr3d_shader_output_dir}"
            COMMAND "${RRR3D_BGFX_SHADERC}"
                --type vertex --platform osx --profile metal -O 3
                --varyingdef
                    "${_rrr3d_shader_source_dir}/varying_legacy_mesh.def.sc"
                -i "${RRR3D_BGFX_SOURCE_DIR}/src"
                -f "${_rrr3d_shader_source_dir}/vs_legacy_mesh.sc"
                -o "${_rrr3d_race_vertex_shader}"
                --bin2c rrr3d_vs_original_race
            DEPENDS
                "${_rrr3d_shader_source_dir}/vs_legacy_mesh.sc"
                "${_rrr3d_shader_source_dir}/varying_legacy_mesh.def.sc"
            VERBATIM)
        add_custom_command(
            OUTPUT "${_rrr3d_race_fragment_shader}"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "${_rrr3d_shader_output_dir}"
            COMMAND "${RRR3D_BGFX_SHADERC}"
                --type fragment --platform osx --profile metal -O 3
                --varyingdef
                    "${_rrr3d_shader_source_dir}/varying_legacy_mesh.def.sc"
                -i "${RRR3D_BGFX_SOURCE_DIR}/src"
                -f "${_rrr3d_shader_source_dir}/fs_legacy_mesh.sc"
                -o "${_rrr3d_race_fragment_shader}"
                --bin2c rrr3d_fs_original_race
            DEPENDS
                "${_rrr3d_shader_source_dir}/fs_legacy_mesh.sc"
                "${_rrr3d_shader_source_dir}/varying_legacy_mesh.def.sc"
            VERBATIM)

        function(_rrr3d_compile_m9_2_shader output type source
                 varying symbol)
            add_custom_command(
                OUTPUT "${output}"
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${_rrr3d_shader_output_dir}"
                COMMAND "${RRR3D_BGFX_SHADERC}"
                    --type "${type}" --platform osx --profile metal -O 3
                    --varyingdef "${varying}"
                    -i "${RRR3D_BGFX_SOURCE_DIR}/src"
                    -f "${source}"
                    -o "${output}"
                    --bin2c "${symbol}"
                DEPENDS "${source}" "${varying}"
                VERBATIM)
        endfunction()

        set(_rrr3d_shadow_vertex_shader
            "${_rrr3d_shader_output_dir}/rrr3d_vs_shadow_map.bin.h")
        set(_rrr3d_shadow_fragment_shader
            "${_rrr3d_shader_output_dir}/rrr3d_fs_shadow_map.bin.h")
        set(_rrr3d_post_vertex_shader
            "${_rrr3d_shader_output_dir}/rrr3d_vs_post_process.bin.h")
        set(_rrr3d_bloom_extract_shader
            "${_rrr3d_shader_output_dir}/rrr3d_fs_bloom_extract.bin.h")
        set(_rrr3d_bloom_blur_shader
            "${_rrr3d_shader_output_dir}/rrr3d_fs_bloom_blur.bin.h")
        set(_rrr3d_tone_map_shader
            "${_rrr3d_shader_output_dir}/rrr3d_fs_tone_map.bin.h")
        set(_rrr3d_water_vertex_shader
            "${_rrr3d_shader_output_dir}/rrr3d_vs_water.bin.h")
        set(_rrr3d_water_fragment_shader
            "${_rrr3d_shader_output_dir}/rrr3d_fs_water.bin.h")
        set(_rrr3d_fog_fragment_shader
            "${_rrr3d_shader_output_dir}/rrr3d_fs_fog_plane.bin.h")
        set(_rrr3d_grass_vertex_shader
            "${_rrr3d_shader_output_dir}/rrr3d_vs_grass_field.bin.h")
        set(_rrr3d_grass_fragment_shader
            "${_rrr3d_shader_output_dir}/rrr3d_fs_grass_field.bin.h")
        set(_rrr3d_sky_vertex_shader
            "${_rrr3d_shader_output_dir}/rrr3d_vs_skybox.bin.h")
        set(_rrr3d_sky_fragment_shader
            "${_rrr3d_shader_output_dir}/rrr3d_fs_skybox.bin.h")
        set(_rrr3d_copy_shader
            "${_rrr3d_shader_output_dir}/rrr3d_fs_copy.bin.h")
        set(_rrr3d_luminance_log_shader
            "${_rrr3d_shader_output_dir}/rrr3d_fs_luminance_log.bin.h")
        set(_rrr3d_luminance_downsample_shader
            "${_rrr3d_shader_output_dir}/rrr3d_fs_luminance_downsample.bin.h")
        set(_rrr3d_luminance_adapt_shader
            "${_rrr3d_shader_output_dir}/rrr3d_fs_luminance_adapt.bin.h")
        set(_rrr3d_legacy_varying
            "${_rrr3d_shader_source_dir}/varying_legacy_mesh.def.sc")
        set(_rrr3d_post_varying
            "${_rrr3d_shader_source_dir}/varying_post_process.def.sc")
        set(_rrr3d_water_varying
            "${_rrr3d_shader_source_dir}/varying_water.def.sc")
        set(_rrr3d_grass_varying
            "${_rrr3d_shader_source_dir}/varying_grass_field.def.sc")
        set(_rrr3d_sky_varying
            "${_rrr3d_shader_source_dir}/varying_skybox.def.sc")
        _rrr3d_compile_m9_2_shader(
            "${_rrr3d_shadow_vertex_shader}" vertex
            "${_rrr3d_shader_source_dir}/vs_shadow_map.sc"
            "${_rrr3d_legacy_varying}" rrr3d_vs_shadow_map)
        _rrr3d_compile_m9_2_shader(
            "${_rrr3d_shadow_fragment_shader}" fragment
            "${_rrr3d_shader_source_dir}/fs_shadow_map.sc"
            "${_rrr3d_legacy_varying}" rrr3d_fs_shadow_map)
        _rrr3d_compile_m9_2_shader(
            "${_rrr3d_post_vertex_shader}" vertex
            "${_rrr3d_shader_source_dir}/vs_post_process.sc"
            "${_rrr3d_post_varying}" rrr3d_vs_post_process)
        _rrr3d_compile_m9_2_shader(
            "${_rrr3d_bloom_extract_shader}" fragment
            "${_rrr3d_shader_source_dir}/fs_bloom_extract.sc"
            "${_rrr3d_post_varying}" rrr3d_fs_bloom_extract)
        _rrr3d_compile_m9_2_shader(
            "${_rrr3d_bloom_blur_shader}" fragment
            "${_rrr3d_shader_source_dir}/fs_bloom_blur.sc"
            "${_rrr3d_post_varying}" rrr3d_fs_bloom_blur)
        _rrr3d_compile_m9_2_shader(
            "${_rrr3d_tone_map_shader}" fragment
            "${_rrr3d_shader_source_dir}/fs_tone_map.sc"
            "${_rrr3d_post_varying}" rrr3d_fs_tone_map)
        _rrr3d_compile_m9_2_shader(
            "${_rrr3d_water_vertex_shader}" vertex
            "${_rrr3d_shader_source_dir}/vs_water.sc"
            "${_rrr3d_water_varying}" rrr3d_vs_water)
        _rrr3d_compile_m9_2_shader(
            "${_rrr3d_water_fragment_shader}" fragment
            "${_rrr3d_shader_source_dir}/fs_water.sc"
            "${_rrr3d_water_varying}" rrr3d_fs_water)
        _rrr3d_compile_m9_2_shader(
            "${_rrr3d_fog_fragment_shader}" fragment
            "${_rrr3d_shader_source_dir}/fs_fog_plane.sc"
            "${_rrr3d_water_varying}" rrr3d_fs_fog_plane)
        _rrr3d_compile_m9_2_shader(
            "${_rrr3d_grass_vertex_shader}" vertex
            "${_rrr3d_shader_source_dir}/vs_grass_field.sc"
            "${_rrr3d_grass_varying}" rrr3d_vs_grass_field)
        _rrr3d_compile_m9_2_shader(
            "${_rrr3d_grass_fragment_shader}" fragment
            "${_rrr3d_shader_source_dir}/fs_grass_field.sc"
            "${_rrr3d_grass_varying}" rrr3d_fs_grass_field)
        _rrr3d_compile_m9_2_shader(
            "${_rrr3d_sky_vertex_shader}" vertex
            "${_rrr3d_shader_source_dir}/vs_skybox.sc"
            "${_rrr3d_sky_varying}" rrr3d_vs_skybox)
        _rrr3d_compile_m9_2_shader(
            "${_rrr3d_sky_fragment_shader}" fragment
            "${_rrr3d_shader_source_dir}/fs_skybox.sc"
            "${_rrr3d_sky_varying}" rrr3d_fs_skybox)
        _rrr3d_compile_m9_2_shader(
            "${_rrr3d_copy_shader}" fragment
            "${_rrr3d_shader_source_dir}/fs_copy.sc"
            "${_rrr3d_post_varying}" rrr3d_fs_copy)
        _rrr3d_compile_m9_2_shader(
            "${_rrr3d_luminance_log_shader}" fragment
            "${_rrr3d_shader_source_dir}/fs_luminance_log.sc"
            "${_rrr3d_post_varying}" rrr3d_fs_luminance_log)
        _rrr3d_compile_m9_2_shader(
            "${_rrr3d_luminance_downsample_shader}" fragment
            "${_rrr3d_shader_source_dir}/fs_luminance_downsample.sc"
            "${_rrr3d_post_varying}" rrr3d_fs_luminance_downsample)
        _rrr3d_compile_m9_2_shader(
            "${_rrr3d_luminance_adapt_shader}" fragment
            "${_rrr3d_shader_source_dir}/fs_luminance_adapt.sc"
            "${_rrr3d_post_varying}" rrr3d_fs_luminance_adapt)
        set(_rrr3d_race_post_shaders
            "${_rrr3d_shadow_vertex_shader}"
            "${_rrr3d_shadow_fragment_shader}"
            "${_rrr3d_post_vertex_shader}"
            "${_rrr3d_bloom_extract_shader}"
            "${_rrr3d_bloom_blur_shader}"
            "${_rrr3d_tone_map_shader}"
            "${_rrr3d_water_vertex_shader}"
            "${_rrr3d_water_fragment_shader}"
            "${_rrr3d_fog_fragment_shader}"
            "${_rrr3d_grass_vertex_shader}"
            "${_rrr3d_grass_fragment_shader}"
            "${_rrr3d_sky_vertex_shader}"
            "${_rrr3d_sky_fragment_shader}"
            "${_rrr3d_copy_shader}"
            "${_rrr3d_luminance_log_shader}"
            "${_rrr3d_luminance_downsample_shader}"
            "${_rrr3d_luminance_adapt_shader}")
    endif()

    if(RRR3D_BUILD_ORIGINAL_MENU)
        set(_rrr3d_bgfx_main main_bgfx_original_menu.cpp)
    elseif(RRR3D_BUILD_PORTABLE_MENU)
        set(_rrr3d_bgfx_main main_bgfx_menu.cpp)
    else()
        set(_rrr3d_bgfx_main main_bgfx.cpp)
    endif()

    set(_rrr3d_executable_options)
    if(APPLE AND RRR3D_BUILD_MACOS_BUNDLE)
        list(APPEND _rrr3d_executable_options MACOSX_BUNDLE)
    endif()

    add_executable(${PROJECT_NAME} ${_rrr3d_executable_options}
        ${_rrr3d_bgfx_main}
        "${_rrr3d_vertex_shader}"
        "${_rrr3d_fragment_shader}"
        ${_rrr3d_race_vertex_shader}
        ${_rrr3d_race_fragment_shader}
        ${_rrr3d_race_post_shaders}
    )

    if(RRR3D_BUILD_ORIGINAL_MENU)
        if(NOT APPLE)
            message(FATAL_ERROR
                "The original Milestone 6 menu currently requires CoreText")
        endif()
        find_library(RRR3D_CORETEXT_FRAMEWORK CoreText REQUIRED)
        find_library(RRR3D_COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
        target_sources(${PROJECT_NAME} PRIVATE
            CoreTextRasterizer.cpp
            CoreTextRasterizer.h
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE
            ${RRR3D_CORETEXT_FRAMEWORK}
            ${RRR3D_COREGRAPHICS_FRAMEWORK}
        )
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            RRR3D_ORIGINAL_MENU=1
        )
    endif()

    if(APPLE AND RRR3D_BUILD_MACOS_BUNDLE)
        set(_rrr3d_bundle_icon "${CMAKE_SOURCE_DIR}/resources/macos/RRR3d.icns")
        set_source_files_properties("${_rrr3d_bundle_icon}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(${PROJECT_NAME} PRIVATE "${_rrr3d_bundle_icon}")

        set(_rrr3d_bundle_license_directory
            "${CMAKE_SOURCE_DIR}/resources/macos/Licenses")
        file(GLOB _rrr3d_bundle_licenses CONFIGURE_DEPENDS
            "${_rrr3d_bundle_license_directory}/*")
        set_source_files_properties(${_rrr3d_bundle_licenses} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/Licenses")
        target_sources(${PROJECT_NAME} PRIVATE ${_rrr3d_bundle_licenses})
        set_target_properties(${PROJECT_NAME} PROPERTIES
            OUTPUT_NAME "RRR3d"
            BUILD_RPATH "@executable_path/../Frameworks"
            INSTALL_RPATH "@executable_path/../Frameworks"
            MACOSX_BUNDLE_INFO_PLIST
                "${CMAKE_SOURCE_DIR}/cmake/macos/Info.plist.in"
            MACOSX_BUNDLE_BUNDLE_NAME "Motor Rock"
            MACOSX_BUNDLE_GUI_IDENTIFIER "org.rrr3d.motorrock"
            MACOSX_BUNDLE_ICON_FILE "RRR3d.icns"
            MACOSX_BUNDLE_INFO_STRING "Motor Rock ${RRR3D_VERSION}"
            MACOSX_BUNDLE_LONG_VERSION_STRING "${RRR3D_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${RRR3D_VERSION}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${RRR3D_VERSION}"
            MACOSX_BUNDLE_COPYRIGHT "RRR3D open-source macOS port"
        )

        set(_rrr3d_codesign_arguments
            --force
            --deep
            --sign "${RRR3D_MACOS_CODESIGN_IDENTITY}")
        if(RRR3D_MACOS_HARDENED_RUNTIME)
            list(APPEND _rrr3d_codesign_arguments --options runtime)
        endif()
        if(RRR3D_MACOS_CODESIGN_IDENTITY STREQUAL "-")
            list(APPEND _rrr3d_codesign_arguments --timestamp=none)
        else()
            list(APPEND _rrr3d_codesign_arguments --timestamp)
        endif()
    endif()
    if(RRR3D_ENABLE_GAMEPAD)
        target_sources(${PROJECT_NAME} PRIVATE
            SdlInputManager.cpp
            SdlInputManager.h
            SdlInputSmoke.cpp
            SdlInputSmoke.h
        )
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            RRR3D_GAMEPAD_INPUT=1
        )
    endif()
    if(RRR3D_ENABLE_AUDIO)
        target_sources(${PROJECT_NAME} PRIVATE
            SdlAudioBackend.cpp
            SdlAudioBackend.h
            SdlAudioSmoke.cpp
            SdlAudioSmoke.h
        )
        if(RRR3D_BUILD_ORIGINAL_MENU)
            target_sources(${PROJECT_NAME} PRIVATE
                OriginalMenuMusic.cpp
                OriginalMenuMusic.h
                OriginalMenuSounds.cpp
                OriginalMenuSounds.h
            )
            if(RRR3D_ENABLE_PHYSICS)
                target_sources(${PROJECT_NAME} PRIVATE
                    OriginalRaceCommentator.cpp
                    OriginalRaceCommentator.h
                    OriginalSpatialAudio.cpp
                    OriginalSpatialAudio.h
                )
            endif()
        endif()
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            RRR3D_AUDIO=1
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE
            ${RRR3D_VORBISFILE_TARGET}
        )
    endif()
    if(RRR3D_ENABLE_VIDEO)
        if(NOT APPLE OR NOT RRR3D_BUILD_ORIGINAL_MENU)
            message(FATAL_ERROR
                "The portable video player requires the original macOS menu")
        endif()
        if(NOT RRR3D_ENABLE_AUDIO)
            message(FATAL_ERROR
                "The source movie flow requires menu audio pause/resume")
        endif()
        if(NOT RRR3D_ENABLE_GAMEPAD OR NOT RRR3D_ENABLE_PHYSICS)
            message(FATAL_ERROR
                "The source movie flow requires the active SDL input/race runtime")
        endif()
        find_library(RRR3D_AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
        find_library(RRR3D_APPKIT_FRAMEWORK AppKit REQUIRED)
        find_library(RRR3D_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
        find_program(RRR3D_FFMPEG_EXECUTABLE NAMES ffmpeg REQUIRED)
        target_sources(${PROJECT_NAME} PRIVATE
            MacVideoPlayer.h
            MacVideoPlayer.mm
        )
        set_source_files_properties(MacVideoPlayer.mm PROPERTIES
            COMPILE_OPTIONS "-fobjc-arc")
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            RRR3D_VIDEO=1
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE
            ${RRR3D_AVFOUNDATION_FRAMEWORK}
            ${RRR3D_APPKIT_FRAMEWORK}
            ${RRR3D_QUARTZCORE_FRAMEWORK}
        )
    endif()
    if(RRR3D_ENABLE_PHYSICS)
        if(RRR3D_BUILD_ORIGINAL_MENU)
            target_sources(${PROJECT_NAME} PRIVATE
                OriginalRaceHud.cpp
                OriginalRaceHud.h
                OriginalRaceRenderer.cpp
                OriginalWorkshopRenderer.cpp
                OriginalRaceRenderer.h
            )
        else()
            target_sources(${PROJECT_NAME} PRIVATE
                PortableRaceRenderer.cpp
                PortableRaceRenderer.h
            )
        endif()
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            RRR3D_PHYSICS=1
        )
    endif()
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${_rrr3d_shader_output_dir}"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${RRR3D_SDL_TARGET}
    )

    if(TARGET Rock3dGame)
        target_link_libraries(${PROJECT_NAME} PRIVATE Rock3dGame)
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE Rock3dEngine)
    endif()

    if(RRR3D_BUILD_BGFX_SCENE)
        set(_rrr3d_m5_car_source
            "${CMAKE_SOURCE_DIR}/resources/game-data/Data/Car")
        set(_rrr3d_m5_assets
            "${_rrr3d_m5_car_source}/buggi.r3d"
            "${_rrr3d_m5_car_source}/buggiWheel.r3d"
            "${_rrr3d_m5_car_source}/buggiWheel.txt"
            "${_rrr3d_m5_car_source}/buggi.dds")
        set_source_files_properties(${_rrr3d_m5_assets}
            PROPERTIES HEADER_FILE_ONLY TRUE)
        target_sources(${PROJECT_NAME} PRIVATE ${_rrr3d_m5_assets})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/game-data/Data/Car"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${_rrr3d_m5_assets}
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/game-data/Data/Car"
            COMMENT "Copying original Buggi .r3d renderer-slice resources"
            VERBATIM)
    endif()

    if(RRR3D_BUILD_PORTABLE_MENU OR RRR3D_BUILD_ORIGINAL_MENU)
        set(_rrr3d_game_data_source "${CMAKE_SOURCE_DIR}/resources/game-data")
        set(_rrr3d_game_data_catalog
            "${_rrr3d_game_data_source}/legacy-assets.catalog")
        file(STRINGS "${_rrr3d_game_data_catalog}"
            _rrr3d_game_data_catalog_entries ENCODING UTF-8)
        set(_rrr3d_game_data_files
            "${_rrr3d_game_data_catalog}"
            "${_rrr3d_game_data_source}/manifest.cfg"
            "${_rrr3d_game_data_source}/menu/menu.cfg"
            "${_rrr3d_game_data_source}/ui/font5x7.txt")
        foreach(_rrr3d_catalog_entry
                IN LISTS _rrr3d_game_data_catalog_entries)
            string(REGEX REPLACE "^[^	]*	" ""
                _rrr3d_catalog_path "${_rrr3d_catalog_entry}")
            list(APPEND _rrr3d_game_data_files
                "${_rrr3d_game_data_source}/${_rrr3d_catalog_path}")
        endforeach()
        set_source_files_properties(${_rrr3d_game_data_files}
            PROPERTIES HEADER_FILE_ONLY TRUE)
        target_sources(${PROJECT_NAME} PRIVATE ${_rrr3d_game_data_files})
        if(RRR3D_ENABLE_VIDEO)
            set(_rrr3d_video_cache_dir
                "${CMAKE_CURRENT_BINARY_DIR}/video-cache")
            set(_rrr3d_video_stems
                Intaria
                Intaria_eng
                Main
                Main_eng
                final
                final_eng
                scHemi5
                scHemi5_eng
                scInferno
                scInferno_eng
                scNho
                scNho_eng
                scPatagonis
                scPatagonis_eng)
            set(_rrr3d_video_cache_files)
            foreach(_rrr3d_video_stem IN LISTS _rrr3d_video_stems)
                set(_rrr3d_video_source
                    "${_rrr3d_game_data_source}/Data/Video/${_rrr3d_video_stem}.avi")
                set(_rrr3d_video_cache
                    "${_rrr3d_video_cache_dir}/${_rrr3d_video_stem}.mp4")
                add_custom_command(
                    OUTPUT "${_rrr3d_video_cache}"
                    COMMAND ${CMAKE_COMMAND} -E make_directory
                        "${_rrr3d_video_cache_dir}"
                    COMMAND "${RRR3D_FFMPEG_EXECUTABLE}"
                        -nostdin -hide_banner -loglevel error -y
                        -fflags +genpts
                        -i "${_rrr3d_video_source}"
                        -map 0:v:0 -map 0:a:0
                        -c:v copy -c:a aac -b:a 192k
                        -avoid_negative_ts make_zero
                        -movflags +faststart
                        "${_rrr3d_video_cache}"
                    DEPENDS "${_rrr3d_video_source}"
                    COMMENT
                        "Preparing original ${_rrr3d_video_stem}.avi H.264/AAC movie for AVFoundation"
                    VERBATIM)
                list(APPEND _rrr3d_video_cache_files
                    "${_rrr3d_video_cache}")
            endforeach()
            add_custom_target(RRR3dVideoCache ALL
                DEPENDS ${_rrr3d_video_cache_files})
            add_dependencies(${PROJECT_NAME} RRR3dVideoCache)
        endif()
        if(APPLE AND RRR3D_BUILD_MACOS_BUNDLE)
            set(_rrr3d_bundle_contents
                "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>")
            set(_rrr3d_bundle_stamp
                "${CMAKE_CURRENT_BINARY_DIR}/RRR3d-bundle-$<CONFIG>.stamp")
            add_custom_command(OUTPUT "${_rrr3d_bundle_stamp}"
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${_rrr3d_bundle_contents}/Frameworks"
                COMMAND ${CMAKE_COMMAND}
                    -Dsource_root=${_rrr3d_game_data_source}
                    -Ddestination_root=${_rrr3d_bundle_contents}/Resources/game-data
                    -P "${CMAKE_SOURCE_DIR}/cmake/copy_catalog_game_data.cmake"
                COMMAND ${CMAKE_COMMAND} -E remove_directory
                    "${_rrr3d_bundle_contents}/Resources/video-cache"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_rrr3d_video_cache_dir}"
                    "${_rrr3d_bundle_contents}/Resources/video-cache"
                COMMAND /usr/bin/xattr -cr
                    "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>"
                COMMAND /usr/bin/codesign ${_rrr3d_codesign_arguments}
                    "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>"
                COMMAND ${CMAKE_COMMAND} -E touch
                    "${_rrr3d_bundle_stamp}"
                DEPENDS
                    ${PROJECT_NAME}
                    ${_rrr3d_game_data_files}
                    ${_rrr3d_video_cache_files}
                    "${CMAKE_SOURCE_DIR}/cmake/copy_catalog_game_data.cmake"
                    "${_rrr3d_bundle_icon}"
                    ${_rrr3d_bundle_licenses}
                COMMENT
                    "Packaging game-data and signing the autonomous RRR3d.app"
                VERBATIM)
            add_custom_target(RRR3dBundle ALL
                DEPENDS "${_rrr3d_bundle_stamp}")
        else()
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND}
                    -Dsource_root=${_rrr3d_game_data_source}
                    -Ddestination_root=$<TARGET_FILE_DIR:${PROJECT_NAME}>/game-data
                    -P "${CMAKE_SOURCE_DIR}/cmake/copy_catalog_game_data.cmake"
                COMMENT "Copying original Motor Rock game-data resources"
                VERBATIM)
            if(RRR3D_ENABLE_VIDEO)
                add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E remove_directory
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/video-cache"
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${_rrr3d_video_cache_dir}"
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/video-cache"
                    COMMENT
                        "Copying AVFoundation-compatible original movie cache"
                    VERBATIM)
            endif()
        endif()
    endif()
elseif(RRR3D_BUILD_SDL_SHELL)
    add_executable(${PROJECT_NAME}
        main_sdl.cpp
        SdlInputAdapter.cpp
        SdlInputAdapter.h
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE ${RRR3D_SDL_TARGET})

    if(TARGET Rock3dGame)
        target_link_libraries(${PROJECT_NAME} PRIVATE Rock3dGame)
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            RRR3D_GAME_CORE_LINKED=1
        )
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE LexStd XPlatform)
    endif()
else()
    message(FATAL_ERROR
        "A non-Windows RRR3d executable requires RRR3D_BUILD_SDL_SHELL=ON"
    )
endif()

rrr3d_set_common_target_options(${PROJECT_NAME})
