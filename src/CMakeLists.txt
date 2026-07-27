if (MSVC)
    # Sources are UTF-8. Without this MSVC decodes them as the system ANSI
    # codepage, which mangles every non-ASCII comment and string literal.
    add_compile_options(/utf-8)

    # Always generate PDBs
    add_compile_options(/Zi)
    add_link_options(/DEBUG)

    add_compile_options(/W3) # Warning level

    add_compile_options("$<$<CONFIG:Release>:/Gy>") # Enable Function-Level Linking
else()
    add_compile_options(-fPIC)
endif()

add_subdirectory(LexStd)
add_subdirectory(MathLib)
add_subdirectory(NetLib)
add_subdirectory(Rock3dEngine)
add_subdirectory(Rock3dGame)
add_subdirectory(RRR3d)

# MapEditor is an MFC application (CWinAppEx, CDockablePane, ribbon resources)
# and has no non-Windows equivalent. It talks to the engine only through
# IWorld/IView, so leaving it out costs the game nothing.
if (WIN32)
    add_subdirectory(MapEditor)
endif()
