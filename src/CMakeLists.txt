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
endif()

add_subdirectory(LexStd)
add_subdirectory(MathLib)
add_subdirectory(NetLib)
add_subdirectory(Rock3dEngine)
add_subdirectory(Rock3dGame)
add_subdirectory(RRR3d)
add_subdirectory(MapEditor)
