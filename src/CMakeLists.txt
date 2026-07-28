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

    # `&D3DXVECTOR3(...)` -- taking the address of a temporary -- is an MSVC
    # extension this codebase uses throughout, almost entirely to pass scratch
    # vectors to D3DX math functions. Clang rejects it by default but
    # materializes the temporary the same way MSVC does when the diagnostic is
    # downgraded, with the same end-of-full-expression lifetime, so the
    # behaviour is identical.
    #
    # Downgrading rather than rewriting the ~40 sites is deliberate: every
    # rewrite is a chance to change evaluation order or scope by hand, for no
    # behavioural gain. Worth cleaning up once the port runs and can be tested.
    # It is safe only because no callee stores the pointer past the call.
    add_compile_options(-Wno-error=address-of-temporary)
endif()

# Windows API substitutes for non-Windows targets
if (NOT WIN32)
    add_subdirectory(XPlatform)
    # extern.7z ships TinyXML only as an MSVC static lib; build it from the
    # vendored 2.5.3 sources instead.
    add_subdirectory(TinyXml)
endif()

# Metal. Apple platforms only; the winemetal ABI this implements is what
# d9mt's D3D9 backend calls.
if (APPLE)
    # OBJC for the vendored d9mtmetal (.m, plain Objective-C where void*
    # converts implicitly); OBJCXX for the bridge itself.
    enable_language(OBJC)
    enable_language(OBJCXX)
    add_subdirectory(MetalBridge)
    # DXVK's D3D9 front-end over d9mt's Metal backend.
    add_subdirectory(D3D9Metal)
    # Backend smoke test -- see its CMakeLists.
    add_subdirectory(D3D9Triangle)
endif()

add_subdirectory(LexStd)
add_subdirectory(MathLib)
add_subdirectory(NetLib)
add_subdirectory(Rock3dEngine)
add_subdirectory(Rock3dGame)
add_subdirectory(RRR3d)

# Runtime tests for the ported layers
enable_testing()
add_subdirectory(Tests)

# MapEditor is an MFC application (CWinAppEx, CDockablePane, ribbon resources)
# and has no non-Windows equivalent. It talks to the engine only through
# IWorld/IView, so leaving it out costs the game nothing.
if (WIN32)
    add_subdirectory(MapEditor)
endif()
