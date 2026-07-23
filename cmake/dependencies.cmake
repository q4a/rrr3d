include(FetchContent)

# PhysX 2.8.4 remains untouched on Windows.  The native macOS race backend
# uses a pinned Jolt release; only the backend adapter sees Jolt types.
if(RRR3D_ENABLE_PHYSICS AND NOT WIN32 AND RRR3D_BUILD_ORIGINAL_MENU)
    set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "Keep project compiler flags" FORCE)
    set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "Jolt warnings" FORCE)
    set(GENERATE_DEBUG_SYMBOLS OFF CACHE BOOL "Jolt debug symbols" FORCE)
    set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "Jolt IPO" FORCE)
    set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "Jolt debug renderer" FORCE)
    set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "Jolt profiler" FORCE)
    set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "Jolt object stream" FORCE)
    set(ENABLE_INSTALL OFF CACHE BOOL "Jolt install targets" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Jolt static library" FORCE)
    FetchContent_Declare(rrr3d_jolt
        URL "https://github.com/jrouwe/JoltPhysics/archive/23dadd0e603f1b321142d4c74df07fce85064989.tar.gz"
        URL_HASH "SHA256=e4a44a9bfdcdb1e47259b7a66a300d9f31daf1b28ec19617761c3aad7165b979"
        SOURCE_SUBDIR Build
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(rrr3d_jolt)
    set(RRR3D_JOLT_TARGET Jolt)
endif()

if(RRR3D_BUILD_SDL_SHELL OR RRR3D_BUILD_DXVK_MOLTENVK_TEST OR
   RRR3D_BUILD_BGFX_SCENE OR RRR3D_BUILD_PORTABLE_MENU OR
   RRR3D_BUILD_ORIGINAL_MENU)
    if(APPLE)
        # Homebrew bottles are built for the host macOS version and can raise
        # the deployment target. Build the pinned source as arm64/macOS 13 so
        # the SDL shell preserves the port's minimum supported OS.
        if(RRR3D_BUILD_DXVK_MOLTENVK_TEST)
            set(SDL_SHARED ON CACHE BOOL "Build SDL3 shared library" FORCE)
        else()
            set(SDL_SHARED OFF CACHE BOOL "Build SDL3 shared library" FORCE)
        endif()
        set(SDL_STATIC ON CACHE BOOL "Build SDL3 static library" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL3 test library" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "Build SDL3 tests" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL3 examples" FORCE)
        set(SDL_INSTALL OFF CACHE BOOL "Install SDL3" FORCE)
        set(SDL_UNINSTALL OFF CACHE BOOL "Add SDL3 uninstall target" FORCE)
        set(SDL_AUDIO ${RRR3D_ENABLE_AUDIO}
            CACHE BOOL "Build SDL3 audio subsystem" FORCE)
        set(SDL_GPU OFF CACHE BOOL "Build SDL3 GPU subsystem" FORCE)
        set(SDL_RENDER OFF CACHE BOOL "Build SDL3 render subsystem" FORCE)
        set(SDL_CAMERA OFF CACHE BOOL "Build SDL3 camera subsystem" FORCE)
        set(SDL_JOYSTICK ${RRR3D_ENABLE_GAMEPAD}
            CACHE BOOL "Build SDL3 joystick subsystem" FORCE)
        set(SDL_HAPTIC OFF CACHE BOOL "Build SDL3 haptic subsystem" FORCE)
        set(SDL_HIDAPI ${RRR3D_ENABLE_GAMEPAD}
            CACHE BOOL "Build SDL3 HIDAPI subsystem" FORCE)
        set(SDL_HIDAPI_LIBUSB OFF
            CACHE BOOL "Use libusb from SDL3 HIDAPI" FORCE)
        set(SDL_POWER OFF CACHE BOOL "Build SDL3 power subsystem" FORCE)
        set(SDL_SENSOR OFF CACHE BOOL "Build SDL3 sensor subsystem" FORCE)
        set(SDL_DIALOG OFF CACHE BOOL "Build SDL3 dialog subsystem" FORCE)
        set(SDL_TRAY OFF CACHE BOOL "Build SDL3 tray subsystem" FORCE)
        set(SDL_OPENGL OFF CACHE BOOL "Build SDL3 OpenGL support" FORCE)
        set(SDL_OPENGLES OFF CACHE BOOL "Build SDL3 OpenGL ES support" FORCE)
        set(SDL_VULKAN ${RRR3D_BUILD_DXVK_MOLTENVK_TEST}
            CACHE BOOL "Build SDL3 Vulkan support" FORCE)
        if(RRR3D_BUILD_DXVK_MOLTENVK_TEST OR RRR3D_BUILD_BGFX_SCENE OR
           RRR3D_BUILD_PORTABLE_MENU OR RRR3D_BUILD_ORIGINAL_MENU)
            set(_rrr3d_sdl_metal ON)
        else()
            set(_rrr3d_sdl_metal OFF)
        endif()
        set(SDL_METAL ${_rrr3d_sdl_metal}
            CACHE BOOL "Build SDL3 Metal view support" FORCE)

        FetchContent_Declare(SDL3
            URL "https://github.com/libsdl-org/SDL/releases/download/release-3.4.12/SDL3-3.4.12.tar.gz"
            URL_HASH "SHA256=f07b958a9ac5020fb7a44cadb957f658b2149c3c8abb4f63145fac9303249db7"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(SDL3)
        set(RRR3D_SDL_TARGET SDL3::SDL3-static)
        if(RRR3D_BUILD_DXVK_MOLTENVK_TEST)
            set(RRR3D_SDL_SHARED_TARGET SDL3::SDL3-shared)
        endif()
    else()
        find_package(SDL3 3.2 CONFIG QUIET)
        if(NOT TARGET SDL3::SDL3)
            message(FATAL_ERROR
                "SDL3 3.2 or newer was not found. Provide SDL3_DIR to CMake."
            )
        endif()
        set(RRR3D_SDL_TARGET SDL3::SDL3)
    endif()
endif()

if(RRR3D_ENABLE_AUDIO)
    # Preserve the game's native Ogg/Vorbis assets without depending on
    # Homebrew dylibs or their host deployment target. Both release archives
    # are pinned by digest and linked statically into the portable executable.
    FetchContent_Declare(rrr3d_ogg_source
        URL "https://downloads.xiph.org/releases/ogg/libogg-1.3.5.tar.xz"
        URL_HASH "SHA256=c4d91be36fc8e54deae7575241e03f4211eb102afb3fc0775fbbc1b740016705"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    if(POLICY CMP0169)
        cmake_policy(PUSH)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_GetProperties(rrr3d_ogg_source)
    if(NOT rrr3d_ogg_source_POPULATED)
        FetchContent_Populate(rrr3d_ogg_source)
    endif()
    if(POLICY CMP0169)
        cmake_policy(POP)
    endif()

    set(_rrr3d_ogg_generated "${CMAKE_CURRENT_BINARY_DIR}/generated/libogg")
    file(MAKE_DIRECTORY "${_rrr3d_ogg_generated}/ogg")
    set(INCLUDE_INTTYPES_H 1)
    set(INCLUDE_STDINT_H 1)
    set(INCLUDE_SYS_TYPES_H 1)
    set(SIZE16 int16_t)
    set(USIZE16 uint16_t)
    set(SIZE32 int32_t)
    set(USIZE32 uint32_t)
    set(SIZE64 int64_t)
    set(USIZE64 uint64_t)
    configure_file(
        "${rrr3d_ogg_source_SOURCE_DIR}/include/ogg/config_types.h.in"
        "${_rrr3d_ogg_generated}/ogg/config_types.h"
        @ONLY
    )
    unset(INCLUDE_INTTYPES_H)
    unset(INCLUDE_STDINT_H)
    unset(INCLUDE_SYS_TYPES_H)
    unset(SIZE16)
    unset(USIZE16)
    unset(SIZE32)
    unset(USIZE32)
    unset(SIZE64)
    unset(USIZE64)
    add_library(rrr3d_ogg STATIC
        "${rrr3d_ogg_source_SOURCE_DIR}/src/bitwise.c"
        "${rrr3d_ogg_source_SOURCE_DIR}/src/framing.c"
    )
    add_library(Ogg::ogg ALIAS rrr3d_ogg)
    target_include_directories(rrr3d_ogg PUBLIC
        "${rrr3d_ogg_source_SOURCE_DIR}/include"
        "${_rrr3d_ogg_generated}"
    )

    FetchContent_Declare(rrr3d_vorbis_source
        URL "https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.tar.xz"
        URL_HASH "SHA256=b33cc4934322bcbf6efcbacf49e3ca01aadbea4114ec9589d1b1e9d20f72954b"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    if(POLICY CMP0169)
        cmake_policy(PUSH)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_GetProperties(rrr3d_vorbis_source)
    if(NOT rrr3d_vorbis_source_POPULATED)
        FetchContent_Populate(rrr3d_vorbis_source)
    endif()
    if(POLICY CMP0169)
        cmake_policy(POP)
    endif()

    set(_rrr3d_vorbis_lib "${rrr3d_vorbis_source_SOURCE_DIR}/lib")
    add_library(rrr3d_vorbis STATIC
        "${_rrr3d_vorbis_lib}/mdct.c"
        "${_rrr3d_vorbis_lib}/smallft.c"
        "${_rrr3d_vorbis_lib}/block.c"
        "${_rrr3d_vorbis_lib}/envelope.c"
        "${_rrr3d_vorbis_lib}/window.c"
        "${_rrr3d_vorbis_lib}/lsp.c"
        "${_rrr3d_vorbis_lib}/lpc.c"
        "${_rrr3d_vorbis_lib}/analysis.c"
        "${_rrr3d_vorbis_lib}/synthesis.c"
        "${_rrr3d_vorbis_lib}/psy.c"
        "${_rrr3d_vorbis_lib}/info.c"
        "${_rrr3d_vorbis_lib}/floor1.c"
        "${_rrr3d_vorbis_lib}/floor0.c"
        "${_rrr3d_vorbis_lib}/res0.c"
        "${_rrr3d_vorbis_lib}/mapping0.c"
        "${_rrr3d_vorbis_lib}/registry.c"
        "${_rrr3d_vorbis_lib}/codebook.c"
        "${_rrr3d_vorbis_lib}/sharedbook.c"
        "${_rrr3d_vorbis_lib}/lookup.c"
        "${_rrr3d_vorbis_lib}/bitrate.c"
    )
    target_include_directories(rrr3d_vorbis
        PUBLIC "${rrr3d_vorbis_source_SOURCE_DIR}/include"
        PRIVATE "${_rrr3d_vorbis_lib}"
    )
    target_link_libraries(rrr3d_vorbis PUBLIC Ogg::ogg)

    add_library(rrr3d_vorbisfile STATIC
        "${_rrr3d_vorbis_lib}/vorbisfile.c"
    )
    target_include_directories(rrr3d_vorbisfile
        PUBLIC "${rrr3d_vorbis_source_SOURCE_DIR}/include"
    )
    target_link_libraries(rrr3d_vorbisfile PUBLIC rrr3d_vorbis)
    set(RRR3D_VORBISFILE_TARGET rrr3d_vorbisfile)
endif()

if(RRR3D_ENABLE_RENDERER AND RRR3D_RENDERER_BACKEND STREQUAL "bgfx")
    include(${CMAKE_CURRENT_LIST_DIR}/bgfx.cmake)
endif()

# glm is header-only. Prefer a package-manager installation on Unix-like hosts,
# while retaining compatibility with the dependency archive used by Windows CI.
find_package(glm 1.0 CONFIG QUIET)
if(APPLE AND TARGET glm::glm-header-only)
    # Homebrew GLM 1.0.3 also exports a compiled dylib built for the host OS.
    # The project uses only headers, so keep the macOS 13 link graph clean.
    set(RRR3D_GLM_TARGET glm::glm-header-only)
elseif(TARGET glm::glm)
    set(RRR3D_GLM_TARGET glm::glm)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/extern/glm/include/glm/glm.hpp")
    add_library(rrr3d_glm INTERFACE)
    target_include_directories(rrr3d_glm INTERFACE
        "${CMAKE_SOURCE_DIR}/extern/glm/include"
    )
    set(RRR3D_GLM_TARGET rrr3d_glm)
else()
    message(FATAL_ERROR
        "glm 1.0 or newer was not found. On macOS install it with "
        "'brew install glm', or provide glm_DIR to CMake."
    )
endif()

# The codebase uses the original TinyXML 1 API, not TinyXML-2. On Windows the
# historical extern archive supplies headers and configuration-specific libs.
# Other platforms build the final TinyXML 1 release from a pinned source URL.
if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/extern/tinyxml/include/tinyxml.h")
    add_library(rrr3d_tinyxml INTERFACE)
    target_include_directories(rrr3d_tinyxml INTERFACE
        "${CMAKE_SOURCE_DIR}/extern/tinyxml/include"
    )
    target_compile_definitions(rrr3d_tinyxml INTERFACE TIXML_USE_STL)
    set(RRR3D_TINYXML_TARGET rrr3d_tinyxml)
else()
    FetchContent_Declare(rrr3d_tinyxml_source
        URL "https://downloads.sourceforge.net/project/tinyxml/tinyxml/2.6.2/tinyxml_2_6_2.tar.gz"
        URL_HASH "SHA256=15bdfdcec58a7da30adc87ac2b078e4417dbe5392f3afb719f9ba6d062645593"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    # TinyXML 1 predates CMake and therefore has no CMakeLists.txt of its own.
    # Populate its sources explicitly, then define the target below.
    if(POLICY CMP0169)
        cmake_policy(PUSH)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_GetProperties(rrr3d_tinyxml_source)
    if(NOT rrr3d_tinyxml_source_POPULATED)
        FetchContent_Populate(rrr3d_tinyxml_source)
    endif()
    if(POLICY CMP0169)
        cmake_policy(POP)
    endif()

    add_library(rrr3d_tinyxml STATIC
        "${rrr3d_tinyxml_source_SOURCE_DIR}/tinystr.cpp"
        "${rrr3d_tinyxml_source_SOURCE_DIR}/tinyxml.cpp"
        "${rrr3d_tinyxml_source_SOURCE_DIR}/tinyxmlerror.cpp"
        "${rrr3d_tinyxml_source_SOURCE_DIR}/tinyxmlparser.cpp"
    )
    target_include_directories(rrr3d_tinyxml PUBLIC
        "${rrr3d_tinyxml_source_SOURCE_DIR}"
    )
    target_compile_definitions(rrr3d_tinyxml PUBLIC TIXML_USE_STL)
    rrr3d_set_common_target_options(rrr3d_tinyxml)
    set(RRR3D_TINYXML_TARGET rrr3d_tinyxml)
endif()
