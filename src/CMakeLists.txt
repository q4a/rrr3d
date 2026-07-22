add_subdirectory(XPlatform)
add_subdirectory(LexStd)
add_subdirectory(MathLib)

if(RRR3D_ENABLE_NETWORK OR (WIN32 AND RRR3D_BUILD_GAME))
    add_subdirectory(NetLib)
endif()

if(RRR3D_BUILD_GAME OR RRR3D_BUILD_BGFX_SCENE OR
   RRR3D_BUILD_PORTABLE_MENU)
    add_subdirectory(Rock3dEngine)
endif()

if(RRR3D_BUILD_GAME)
    add_subdirectory(Rock3dGame)
endif()

if(RRR3D_BUILD_GAME OR RRR3D_BUILD_SDL_SHELL OR RRR3D_BUILD_BGFX_SCENE OR
   RRR3D_BUILD_PORTABLE_MENU)
    add_subdirectory(RRR3d)
endif()

if(RRR3D_BUILD_DXVK_MOLTENVK_TEST)
    add_subdirectory(RendererSpike)
endif()

if(RRR3D_BUILD_MAP_EDITOR)
    if(WIN32)
        add_subdirectory(MapEditor)
    else()
        message(WARNING "MapEditor requires MFC and is disabled on non-Windows platforms")
    endif()
endif()
