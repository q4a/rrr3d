project(XPlatform)

list(APPEND PROJECT_DIRS
    "header"
    "source"
    )
add_dir("${PROJECT_DIRS}" "${PROJECT_NAME}")

# SHARED, deliberately, and this is not a preference.
#
# A static archive is copied into every target that links it, so each one gets
# its own copy of this library's globals -- the last-error slot, the process
# start time, and everything that lands here later. The engine is split across
# an executable and a shared library, so with a static archive the two halves
# disagree about state that is supposed to be process-wide, silently. Windows
# hides this behind its DLL/LIB split; a port that turns .lib into .a inherits
# it.
add_library(${PROJECT_NAME} SHARED
    ${${PROJECT_NAME}_SOURCE}
    ${${PROJECT_NAME}_INLINE}
    ${${PROJECT_NAME}_HEADER}
)

# PUBLIC: everything that links XPlatform is including <windows.h> and expecting
# to find this one.
target_include_directories(${PROJECT_NAME} PUBLIC
    header
    # Vendored by tools/vendor-directx-headers.py. Both are include roots in
    # their own right because d3d9.h includes <objbase.h> and <windows.h> by
    # those bare names, exactly as it does on Windows.
    header/windows
    header/directx
)

# SDL3, PRIVATE. Input and window state are what XPlatform cannot answer from a
# handle -- the HWND the engine holds is a CAMetalLayer -- so GetAsyncKeyState,
# GetCursorPos and XInputGetState read from SDL instead. Nothing in the headers
# mentions it: the scancode table is declared with plain ints precisely so
# <SDL3/SDL.h> stays out of every consumer.
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)

target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::SDL3)

# FAudio, behind xaudio2.h -- see source/xaudio2_faudio.cpp.
#
# FAudio ships no xaudio2.h of its own: it reimplements the same engine with
# the same semantics but exposes it as a C API, and on Wine it is Wine's own
# xaudio2 DLL that puts the IXAudio2 interfaces on top. That layer is ours, and
# it is confined to this target -- nothing downstream learns FAudio exists.
pkg_check_modules(FAUDIO REQUIRED IMPORTED_TARGET FAudio)

target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::FAUDIO)

# d3dx9.h reaches for the D3DX math declarations, which live in MathLib.
# Headers only, deliberately: MathLib links XPlatform, so a link dependency the
# other way would be circular.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/MathLib/header
)

# libvkd3d-shader: the HLSL compiler behind D3DXCreateEffect.
#
# Chosen because its d3dbc writer emits a standard constant table, so parameter
# names map to registers the way D3DX does -- which is what makes ID3DXEffect
# implementable from the compiler's output rather than guesswork. See
# d3dx_effect.cpp and tools/setup-vkd3d-macos.sh.
#
# PRIVATE: this library is SHARED, so the static archive is absorbed here and
# nothing downstream needs to know vkd3d exists.
set(VKD3D ${CMAKE_SOURCE_DIR}/extern/vkd3d)

if (NOT EXISTS ${VKD3D}/build/.libs/libvkd3d-shader.a)
    message(FATAL_ERROR
        "libvkd3d-shader not found. Run tools/setup-vkd3d-macos.sh")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${VKD3D}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE ${VKD3D}/build/.libs/libvkd3d-shader.a)
