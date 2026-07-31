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
