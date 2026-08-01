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
