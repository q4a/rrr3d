if (MSVC)
    # Always generate PDBs
    add_compile_options(/Zi)
    add_link_options(/DEBUG)

    add_compile_options(/W3) # Warning level

    # The sources are UTF-8 (see tools/transcode-cp1251.py). Without this MSVC
    # reads them as the system ANSI codepage and mangles every non-ASCII byte,
    # so the transcode and this flag are a single change and cannot be split.
    add_compile_options(/utf-8)

    add_compile_options("$<$<CONFIG:Release>:/Gy>") # Enable Function-Level Linking

    # CMake's Debug configuration defines neither NDEBUG nor _DEBUG, and the
    # PhysX headers require exactly one of them. MSVC's own Debug runtime
    # defines _DEBUG, so match it everywhere rather than only where it happens
    # to arrive by accident.
    add_compile_definitions("$<$<CONFIG:Debug>:_DEBUG>")
else()
    add_compile_options(-fPIC)

    # Same reasoning as the MSVC branch above -- the engine's own LSL_ASSERT and
    # the PhysX headers both key off _DEBUG.
    add_compile_definitions("$<$<CONFIG:Debug>:_DEBUG>")
endif()

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(LIBSUFFIX "32")
else()
    set(LIBSUFFIX "64")
endif()
message(STATUS "LIBSUFFIX=${LIBSUFFIX}")
message(STATUS "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")

# The Win32 substitute layer. On Windows the real headers are used and this
# whole target is absent, so nothing here can shadow them.
if (NOT MSVC)
    add_subdirectory(XPlatform)
endif()

# Which PhysX the engine compiles against.
#
# The option exists so that Windows -- deferred, not dropped -- can keep
# building against the real 2.8 SDK while macOS uses the shim, and so that
# turning it on there later is a flip rather than a redesign.
#
# Off Windows there is no choice to make: the 2.8 headers do not compile at all
# on macOS (Nx.h stops with "Platforms pointer size ambiguous", NxMath.h wants a
# Linux variant that does not ship), so the shim is forced on.
#
# Declared before add_subdirectory(PhysX28) because it now gates it.
if (APPLE)
    set(RRR3D_PHYSX28_SHIM ON)
else()
    option(RRR3D_PHYSX28_SHIM "Use the PhysX 2.8 shim instead of the real SDK" OFF)
    if (NOT DEFINED RRR3D_PHYSX_VER)
        set(RRR3D_PHYSX_VER "284")
    endif()
endif()

# The PhysX 2.8 compatibility shim. Independent of the rest of the tree -- it
# implements an API rather than consuming one -- so it builds and tests on its
# own, before anything that uses it compiles.
#
# Only when it is actually wanted. The shim is built on Bullet and its
# CMakeLists does pkg_check_modules(BULLET REQUIRED), which hard-fails configure
# on a machine without pkg-config and Bullet -- i.e. every Windows runner. A
# Windows build using the real SDK has no business demanding the shim's
# dependencies.
if (RRR3D_PHYSX28_SHIM)
    add_subdirectory(PhysX28)
endif()

if (RRR3D_PHYSX28_SHIM)
    set(RRR3D_PHYSX_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/src/PhysX28/include)
    message(STATUS "physics: PhysX 2.8 shim (over Bullet)")
else()
    if (RRR3D_PHYSX_VER STREQUAL "283")
        add_definitions(-DRRR3D_PHYSX_283)
    endif()
    set(RRR3D_PHYSX_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/extern/physx${RRR3D_PHYSX_VER}/include
        ${CMAKE_SOURCE_DIR}/extern/physx${RRR3D_PHYSX_VER}/include/Cooking
        ${CMAKE_SOURCE_DIR}/extern/physx${RRR3D_PHYSX_VER}/include/Foundation
        ${CMAKE_SOURCE_DIR}/extern/physx${RRR3D_PHYSX_VER}/include/Physics
        ${CMAKE_SOURCE_DIR}/extern/physx${RRR3D_PHYSX_VER}/include/PhysXLoader
    )
    message(STATUS "physics: real PhysX ${RRR3D_PHYSX_VER} SDK")
endif()

if (NOT MSVC)
    add_subdirectory(Tests)

    # The FAudio resampler reproducer. Here rather than beside the graphics
    # tests because it needs nothing but XPlatform, which is what makes it cheap
    # enough to run under the sanitizer on every build.
    add_subdirectory(AudioSweep)

    # The cutscene demuxer and decoder, likewise without the game. Apple-only,
    # because VideoToolbox is.
    if (APPLE)
    add_subdirectory(VideoProbe)

    # The Metal backend. Apple-only by construction, and absent on Windows
    # where the real D3D9 runtime is what draws.
    add_subdirectory(MetalBridge)

    # D3D9 over it, from the trees tools/setup-d3d9metal-macos.sh fetches. Only
    # if they are actually there: the rest of the tree builds and its tests run
    # without them, and failing to configure would hide that.
    if (EXISTS ${CMAKE_SOURCE_DIR}/extern/d9mt/vendor/dxvk/src/d3d9/d3d9_device.cpp)
        add_subdirectory(D3D9Metal)
        add_subdirectory(D3D9Triangle)

        # Dear ImGui, and the one test that says whether its D3D9 backend works
        # here. Both only if it has been fetched -- the rest of the tree builds
        # and its tests run without it, which is the same rule extern/d9mt gets.
        if (EXISTS ${CMAKE_SOURCE_DIR}/extern/imgui/imgui.cpp)
            add_subdirectory(ImGui)
            add_subdirectory(D3D9ImGui)
        else()
            message(STATUS "imgui: extern/ not populated -- "
                           "run tools/setup-imgui.py for the map editor")
        endif()
    else()
        message(STATUS "d3d9metal: extern/ not populated -- "
                       "run tools/setup-d3d9metal-macos.sh for the D3D9 backend")
    endif()
    endif()
endif()

add_subdirectory(TinyXml)
add_subdirectory(LexStd)
add_subdirectory(MathLib)
add_subdirectory(NetLib)
add_subdirectory(Rock3dEngine)
add_subdirectory(Rock3dGame)
add_subdirectory(RRR3d)

# The map editor: MFC on Windows, Dear ImGui everywhere else. Both live in the
# same directory and the CMakeLists there chooses.
add_subdirectory(MapEditor)
