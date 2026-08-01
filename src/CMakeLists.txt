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

# The Win32 substitute layer. On Windows the real headers are used and this
# whole target is absent, so nothing here can shadow them.
if (NOT MSVC)
    add_subdirectory(XPlatform)
endif()

# The PhysX 2.8 compatibility shim. Independent of the rest of the tree -- it
# implements an API rather than consuming one -- so it builds and tests on its
# own, before anything that uses it compiles.
add_subdirectory(PhysX28)

# Which PhysX the engine compiles against.
#
# The option exists so that Windows -- deferred, not dropped -- can keep
# building against the real 2.8 SDK while macOS uses the shim, and so that
# turning it on there later is a flip rather than a redesign.
#
# Off Windows there is no choice to make: the 2.8 headers do not compile at all
# on macOS (Nx.h stops with "Platforms pointer size ambiguous", NxMath.h wants a
# Linux variant that does not ship), so the shim is forced on.
if (MSVC)
    option(RRR3D_PHYSX28_SHIM "Use the PhysX 2.8 shim instead of the real SDK" OFF)
else()
    set(RRR3D_PHYSX28_SHIM ON)
endif()

if (RRR3D_PHYSX28_SHIM)
    set(RRR3D_PHYSX_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/src/PhysX28/include)
    message(STATUS "physics: PhysX 2.8 shim (over Bullet)")
else()
    set(RRR3D_PHYSX_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/extern/physx/include/Cooking
        ${CMAKE_SOURCE_DIR}/extern/physx/include/Foundation
        ${CMAKE_SOURCE_DIR}/extern/physx/include/Physics
        ${CMAKE_SOURCE_DIR}/extern/physx/include/PhysXLoader
    )
    message(STATUS "physics: real PhysX 2.8 SDK")
endif()

if (NOT MSVC)
    add_subdirectory(Tests)
endif()

add_subdirectory(TinyXml)
add_subdirectory(LexStd)
add_subdirectory(MathLib)
add_subdirectory(NetLib)
add_subdirectory(Rock3dEngine)
add_subdirectory(Rock3dGame)
add_subdirectory(RRR3d)

# MFC, and Windows-only by construction. It moves to Dear ImGui later in the
# port; until then it is simply absent off Windows.
if (MSVC)
    add_subdirectory(MapEditor)
endif()
