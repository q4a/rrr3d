if(NOT APPLE)
    message(FATAL_ERROR
        "rrr3d_dxvk_moltenvk_test is an Apple Silicon feasibility target")
endif()

if(NOT RRR3D_DXVK_NATIVE_ROOT)
    message(FATAL_ERROR
        "Set RRR3D_DXVK_NATIVE_ROOT to the install directory produced by "
        "scripts/build_dxvk_macos.sh")
endif()

# These are derived from user-selectable prefixes. Re-resolve them whenever a
# configure switches between an experimental install and the pinned preset.
unset(RRR3D_DXVK_INCLUDE_DIR CACHE)
unset(RRR3D_DXVK_D3D9_LIBRARY CACHE)
unset(RRR3D_VULKAN_INCLUDE_DIR CACHE)
unset(RRR3D_MOLTENVK_LIBRARY CACHE)

find_path(RRR3D_DXVK_INCLUDE_DIR
    NAMES d3d9.h
    PATHS "${RRR3D_DXVK_NATIVE_ROOT}/include/dxvk"
    NO_DEFAULT_PATH
)

find_library(RRR3D_DXVK_D3D9_LIBRARY
    NAMES dxvk_d3d9.0
    PATHS
        "${RRR3D_DXVK_NATIVE_ROOT}/lib"
        "${RRR3D_DXVK_NATIVE_ROOT}/lib64"
    NO_DEFAULT_PATH
)

set(_rrr3d_moltenvk_hints)
if(RRR3D_MOLTENVK_ROOT)
    list(APPEND _rrr3d_moltenvk_hints "${RRR3D_MOLTENVK_ROOT}")
endif()
list(APPEND _rrr3d_moltenvk_hints /opt/homebrew /usr/local)

find_path(RRR3D_VULKAN_INCLUDE_DIR
    NAMES vulkan/vulkan.h
    HINTS ${_rrr3d_moltenvk_hints}
    PATH_SUFFIXES include
)

find_library(RRR3D_MOLTENVK_LIBRARY
    NAMES MoltenVK
    HINTS ${_rrr3d_moltenvk_hints}
    PATH_SUFFIXES lib
)

if(NOT RRR3D_DXVK_INCLUDE_DIR OR NOT RRR3D_DXVK_D3D9_LIBRARY)
    message(FATAL_ERROR
        "Pinned DXVK Native headers/library were not found below "
        "RRR3D_DXVK_NATIVE_ROOT='${RRR3D_DXVK_NATIVE_ROOT}'")
endif()

if(NOT RRR3D_VULKAN_INCLUDE_DIR OR NOT RRR3D_MOLTENVK_LIBRARY)
    message(FATAL_ERROR
        "MoltenVK and Vulkan headers were not found. Set RRR3D_MOLTENVK_ROOT.")
endif()

if(NOT RRR3D_SDL_SHARED_TARGET OR
   NOT TARGET "${RRR3D_SDL_SHARED_TARGET}")
    message(FATAL_ERROR "The renderer spike requires the pinned SDL3 shared target")
endif()

add_executable(rrr3d_dxvk_moltenvk_test dxvk_moltenvk_test.cpp)
target_include_directories(rrr3d_dxvk_moltenvk_test SYSTEM PRIVATE
    "${RRR3D_DXVK_INCLUDE_DIR}"
    "${RRR3D_VULKAN_INCLUDE_DIR}"
)
target_link_libraries(rrr3d_dxvk_moltenvk_test PRIVATE
    ${RRR3D_SDL_SHARED_TARGET}
    "${RRR3D_DXVK_D3D9_LIBRARY}"
    "${RRR3D_MOLTENVK_LIBRARY}"
)
rrr3d_set_common_target_options(rrr3d_dxvk_moltenvk_test)

set_target_properties(rrr3d_dxvk_moltenvk_test PROPERTIES
    BUILD_RPATH "@executable_path"
    INSTALL_RPATH "@executable_path"
)

# Homebrew's MoltenVK dylib uses an absolute install name. Discover the exact
# name instead of baking a Cellar/opt path into the executable.
execute_process(
    COMMAND /usr/bin/otool -D "${RRR3D_MOLTENVK_LIBRARY}"
    OUTPUT_VARIABLE _rrr3d_moltenvk_otool
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)
string(REGEX REPLACE "^[^\n]*\n[ \t]*([^\n]+).*$" "\\1"
    RRR3D_MOLTENVK_INSTALL_NAME "${_rrr3d_moltenvk_otool}")

execute_process(
    COMMAND /usr/bin/otool -D "${RRR3D_DXVK_D3D9_LIBRARY}"
    OUTPUT_VARIABLE _rrr3d_dxvk_otool
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)
string(REGEX REPLACE "^[^\n]*\n[ \t]*([^\n]+).*$" "\\1"
    RRR3D_DXVK_INSTALL_NAME "${_rrr3d_dxvk_otool}")

add_custom_command(TARGET rrr3d_dxvk_moltenvk_test POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${RRR3D_DXVK_D3D9_LIBRARY}"
        "$<TARGET_FILE_DIR:rrr3d_dxvk_moltenvk_test>/libdxvk_d3d9.0.dylib"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:${RRR3D_SDL_SHARED_TARGET}>"
        "$<TARGET_FILE_DIR:rrr3d_dxvk_moltenvk_test>/libSDL3.0.dylib"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${RRR3D_MOLTENVK_LIBRARY}"
        "$<TARGET_FILE_DIR:rrr3d_dxvk_moltenvk_test>/libMoltenVK.dylib"
    COMMAND "${CMAKE_INSTALL_NAME_TOOL}" -change
        "${RRR3D_DXVK_INSTALL_NAME}"
        "@rpath/libdxvk_d3d9.0.dylib"
        "$<TARGET_FILE:rrr3d_dxvk_moltenvk_test>"
    COMMAND "${CMAKE_INSTALL_NAME_TOOL}" -change
        "${RRR3D_MOLTENVK_INSTALL_NAME}"
        "@rpath/libMoltenVK.dylib"
        "$<TARGET_FILE:rrr3d_dxvk_moltenvk_test>"
    COMMAND "${CMAKE_INSTALL_NAME_TOOL}" -id
        "@rpath/libdxvk_d3d9.0.dylib"
        "$<TARGET_FILE_DIR:rrr3d_dxvk_moltenvk_test>/libdxvk_d3d9.0.dylib"
    COMMAND "${CMAKE_INSTALL_NAME_TOOL}" -id
        "@rpath/libMoltenVK.dylib"
        "$<TARGET_FILE_DIR:rrr3d_dxvk_moltenvk_test>/libMoltenVK.dylib"
    COMMAND /usr/bin/codesign --force --sign -
        "$<TARGET_FILE_DIR:rrr3d_dxvk_moltenvk_test>/libdxvk_d3d9.0.dylib"
    COMMAND /usr/bin/codesign --force --sign -
        "$<TARGET_FILE_DIR:rrr3d_dxvk_moltenvk_test>/libSDL3.0.dylib"
    COMMAND /usr/bin/codesign --force --sign -
        "$<TARGET_FILE_DIR:rrr3d_dxvk_moltenvk_test>/libMoltenVK.dylib"
    COMMAND /usr/bin/codesign --force --sign -
        "$<TARGET_FILE:rrr3d_dxvk_moltenvk_test>"
    VERBATIM
)

add_custom_target(run_rrr3d_dxvk_moltenvk_test
    COMMAND "${CMAKE_COMMAND}" -E env
        DXVK_WSI_DRIVER=SDL3
        DXVK_LOG_LEVEL=info
        DXVK_LOG_PATH=none
        "$<TARGET_FILE:rrr3d_dxvk_moltenvk_test>"
    DEPENDS rrr3d_dxvk_moltenvk_test
    USES_TERMINAL
)

unset(_rrr3d_moltenvk_hints)
unset(_rrr3d_moltenvk_otool)
unset(_rrr3d_dxvk_otool)
