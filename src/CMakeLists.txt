if (MSVC)
    # Always generate PDBs
    add_compile_options(/Zi)
    add_link_options(/DEBUG)

    add_compile_options(/W3) # Warning level

    add_compile_options("$<$<CONFIG:Release>:/Gy>") # Enable Function-Level Linking
else()
    add_compile_options(-fpermissive)
endif()

add_subdirectory(LexStd)
add_subdirectory(MathLib)
add_subdirectory(NetLib)
#if (NOT MSVC)
#    add_subdirectory(XPlatform)
#endif()
add_subdirectory(Rock3dEngine)
add_subdirectory(Rock3dGame)
add_subdirectory(RRR3d)
add_subdirectory(MapEditor)
