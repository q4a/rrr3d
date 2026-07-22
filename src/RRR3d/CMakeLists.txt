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
elseif(RRR3D_BUILD_PORTABLE_MENU OR RRR3D_BUILD_BGFX_SCENE)
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

    if(RRR3D_BUILD_PORTABLE_MENU)
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
    )

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
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            RRR3D_AUDIO=1
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE
            ${RRR3D_VORBISFILE_TARGET}
        )
    endif()
    if(RRR3D_ENABLE_PHYSICS)
        target_sources(${PROJECT_NAME} PRIVATE
            PortableRaceRenderer.cpp
            PortableRaceRenderer.h
        )
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

    if(RRR3D_BUILD_PORTABLE_MENU)
        set(_rrr3d_game_data_source "${CMAKE_SOURCE_DIR}/resources/game-data")
        file(GLOB_RECURSE _rrr3d_game_data_files CONFIGURE_DEPENDS
            "${_rrr3d_game_data_source}/*")
        set_source_files_properties(${_rrr3d_game_data_files}
            PROPERTIES HEADER_FILE_ONLY TRUE)
        target_sources(${PROJECT_NAME} PRIVATE ${_rrr3d_game_data_files})
        if(APPLE AND RRR3D_BUILD_MACOS_BUNDLE)
            set(_rrr3d_bundle_contents
                "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>")
            set(_rrr3d_bundle_stamp
                "${CMAKE_CURRENT_BINARY_DIR}/RRR3d-bundle-$<CONFIG>.stamp")
            add_custom_command(OUTPUT "${_rrr3d_bundle_stamp}"
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${_rrr3d_bundle_contents}/Frameworks"
                COMMAND ${CMAKE_COMMAND} -E remove_directory
                    "${_rrr3d_bundle_contents}/Resources/game-data"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_rrr3d_game_data_source}"
                    "${_rrr3d_bundle_contents}/Resources/game-data"
                COMMAND /usr/bin/xattr -cr
                    "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>"
                COMMAND /usr/bin/codesign ${_rrr3d_codesign_arguments}
                    "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>"
                COMMAND ${CMAKE_COMMAND} -E touch
                    "${_rrr3d_bundle_stamp}"
                DEPENDS
                    ${PROJECT_NAME}
                    ${_rrr3d_game_data_files}
                    "${_rrr3d_bundle_icon}"
                    ${_rrr3d_bundle_licenses}
                COMMENT
                    "Packaging game-data and signing the autonomous RRR3d.app"
                VERBATIM)
            add_custom_target(RRR3dBundle ALL
                DEPENDS "${_rrr3d_bundle_stamp}")
        else()
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E remove_directory
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/game-data"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_rrr3d_game_data_source}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/game-data"
                COMMENT "Copying validated portable game-data resources"
                VERBATIM)
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
