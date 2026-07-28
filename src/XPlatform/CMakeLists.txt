project(XPlatform)

list(APPEND PROJECT_DIRS
    "header"
    "source"
    )
add_dir("${PROJECT_DIRS}" "${PROJECT_NAME}")

# SHARED, not STATIC, and that distinction is load-bearing.
#
# This library holds process-wide state: the registered client size, and the
# map VirtualFree uses to recover an allocation's length. Linked statically it
# is duplicated into every module that uses it -- the executable and
# libRock3dGame.dylib each got their own copy -- so a value written through one
# copy is invisible through the other.
#
# That is not theoretical. The SDL shell lives in the executable and published
# the window size there; View::GetWndSize runs in the dylib and read its own
# copy, still zero. ScreenToView divides the mouse position by it, so every
# click arrived at INT_MAX and the game ignored the mouse entirely, silently.
# The VirtualAlloc size map has the same hazard, where the consequence would be
# a failed VirtualFree rather than a visible symptom.
add_library(${PROJECT_NAME} SHARED
    ${${PROJECT_NAME}_SOURCE}
    ${${PROJECT_NAME}_INLINE}
    ${${PROJECT_NAME}_HEADER}
)

target_include_directories(${PROJECT_NAME} PUBLIC
    header
    # Vendored: dxvk's native Windows/COM headers and MinGW-w64's DirectX
    # headers -- the same pairing dxvk native uses to build D3D9 code off
    # Windows. See the licence files in each directory.
    header/windows
    header/directx
)

# d3dx9.h reaches for the Wine d3dx9 math headers, which live in MathLib.
# Headers only, deliberately: MathLib links XPlatform, so a link dependency
# the other way would be circular.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/MathLib/header
)

if (APPLE)
    # The HLSL compiler behind D3DXCreateEffect -- see d3dx_effect.cpp and
    # tools/setup-vkd3d-macos.sh. PUBLIC because this is a static library:
    # whatever links XPlatform is what resolves these symbols.
    set(VKD3D ${CMAKE_SOURCE_DIR}/extern/vkd3d)

    if (NOT EXISTS ${VKD3D}/build/.libs/libvkd3d-shader.a)
        message(FATAL_ERROR
            "libvkd3d-shader not found. Run tools/setup-vkd3d-macos.sh")
    endif()

    target_include_directories(${PROJECT_NAME} PRIVATE ${VKD3D}/include)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${VKD3D}/build/.libs/libvkd3d-shader.a)
endif()
