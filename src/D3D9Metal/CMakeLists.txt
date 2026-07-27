project(D3D9Metal)

# Direct3D 9 on Metal: DXVK's D3D9 front-end over d9mt's Metal DxvkContext.
# See README.md for the layout and for what was changed in the vendored trees.
#
# The source list is d9mt's own, from its scripts/build-dxvkfe.sh, which is the
# authority on which DXVK translation units this configuration needs -- most of
# dxvk/src/dxvk is header-only here because d3d9fe supplies the out-of-line
# bodies that upstream implements against Vulkan.

set(V ${CMAKE_CURRENT_SOURCE_DIR}/dxvk)
set(SC ${CMAKE_CURRENT_SOURCE_DIR}/spirv-cross)

file(GLOB D3D9_SOURCES ${V}/src/d3d9/*.cpp)
file(GLOB DXSO_SOURCES ${V}/src/dxso/*.cpp)
file(GLOB FE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/d3d9fe/*.cpp)

add_library(${PROJECT_NAME} STATIC
    ${D3D9_SOURCES}
    ${DXSO_SOURCES}
    ${FE_SOURCES}

    ${V}/src/spirv/spirv_code_buffer.cpp
    ${V}/src/spirv/spirv_compression.cpp
    ${V}/src/spirv/spirv_module.cpp

    # CPU-side helpers only: the format table, state normalisation and the
    # command-stream thread. The GPU-facing DxvkContext is d3d9fe's.
    ${V}/src/dxvk/dxvk_shader.cpp
    ${V}/src/dxvk/dxvk_pipelayout.cpp
    ${V}/src/dxvk/dxvk_shader_key.cpp
    ${V}/src/dxvk/dxvk_format.cpp
    ${V}/src/dxvk/dxvk_util.cpp
    ${V}/src/dxvk/dxvk_cs.cpp
    ${V}/src/dxvk/dxvk_constant_state.cpp

    ${V}/src/util/thread.cpp
    ${V}/src/util/util_env.cpp
    ${V}/src/util/util_string.cpp
    ${V}/src/util/util_flush.cpp
    ${V}/src/util/util_gdi.cpp
    ${V}/src/util/util_luid.cpp
    ${V}/src/util/util_matrix.cpp
    ${V}/src/util/util_shared_res.cpp
    ${V}/src/util/util_fps_limiter.cpp
    ${V}/src/util/util_sleep.cpp
    ${V}/src/util/log/log.cpp
    ${V}/src/util/log/log_debug.cpp
    ${V}/src/util/config/config.cpp
    ${V}/src/util/sha1/sha1_util.cpp
    ${V}/src/util/sha1/sha1.c
    ${V}/src/util/sync/sync_recursive.cpp
    ${V}/src/util/com/com_guid.cpp
    ${V}/src/util/com/com_private_data.cpp

    # wsi_platform only: the win32 driver files are Win32 throughout, and
    # d9mt installs its own driver over the bootstrap entry supplied by
    # d3d9fe/d9mt_wsi_bootstrap.cpp.
    ${V}/src/wsi/wsi_platform.cpp

    # Enum name tables; the loader itself is stubbed in d3d9fe/stubs.cpp.
    ${V}/src/vulkan/vulkan_names.cpp

    # SPIR-V to MSL, verbatim Khronos.
    ${SC}/spirv_cross.cpp
    ${SC}/spirv_parser.cpp
    ${SC}/spirv_cross_parsed_ir.cpp
    ${SC}/spirv_cfg.cpp
    ${SC}/spirv_glsl.cpp
    ${SC}/spirv_msl.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${V}/src
    # Generated shader headers sit beside the sources that include them, and
    # some do so with angle brackets -- which only searches -I paths.
    ${V}/src/dxvk
    ${V}/src/d3d9
    ${V}/src/dxso
    ${V}/include
    ${V}/include/vulkan/include
    ${V}/include/spirv/include
    ${CMAKE_CURRENT_SOURCE_DIR}/d3d9fe
    ${CMAKE_CURRENT_SOURCE_DIR}/winemetal
    ${CMAKE_CURRENT_SOURCE_DIR}/generated
    ${SC}
)

# DXVK selects a window-system driver from a table guarded by these. d9mt
# repoints the Win32 entry at its own factory rather than editing the vendored
# tree, so the define stays on and the entry comes from d9mt_wsi_bootstrap.cpp.
target_compile_definitions(${PROJECT_NAME} PRIVATE DXVK_WSI_WIN32)

target_link_libraries(${PROJECT_NAME} PUBLIC XPlatform MetalBridge)

# Both trees expect <windows.h> to have been seen already.
target_compile_options(${PROJECT_NAME} PRIVATE -include xplatform.h)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)

# Vendored third-party: upstream's warnings are not this project's to fix.
target_compile_options(${PROJECT_NAME} PRIVATE -w)
