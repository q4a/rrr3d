project(D3D9Metal)

# Direct3D 9 on Metal: DXVK's D3D9 front-end over d9mt's Metal DxvkContext.
#
#   D3D9 API -> DXVK front-end -> DxvkContext -> d9mt's Metal backend
#            -> winemetal ABI -> src/MetalBridge -> Metal
#
# Fetched and patched into extern/ by tools/setup-d3d9metal-macos.sh -- nothing
# here is checked in, which is why this directory holds a CMakeLists and nothing
# else. Run that script first.
#
# One tree, not two: d9mt carries its own vendored DXVK v2.7.1 snapshot, and
# that is the configuration it supports. The doubled include paths below
# (include/spirv/include) are not a mistake -- DXVK mounts the Khronos header
# repos at directories named for what they provide, and those repos already
# have an include/ prefix of their own.
#
# The source list is d9mt's own, from its scripts/build-dxvkfe.sh, which is the
# authority on which DXVK translation units this configuration needs. Most of
# dxvk/src/dxvk is header-only here because d3d9fe supplies the out-of-line
# bodies that upstream implements against Vulkan.

set(D9 ${CMAKE_SOURCE_DIR}/extern/d9mt)
# DXVK comes from d9mt's own vendored snapshot -- see the setup script for why
# there is only one copy.
set(V ${D9}/vendor/dxvk)
set(SC ${D9}/vendor/spirv-cross)

if (NOT EXISTS ${V}/src/d3d9/d3d9_device.cpp OR NOT EXISTS ${D9}/src/d3d9fe/d9mt_device.cpp)
    message(FATAL_ERROR
        "extern/d9mt is missing or incomplete.\n"
        "Run tools/setup-d3d9metal-macos.sh first.")
endif()

file(GLOB D3D9_SOURCES ${V}/src/d3d9/*.cpp)
file(GLOB DXSO_SOURCES ${V}/src/dxso/*.cpp)
file(GLOB FE_SOURCES ${D9}/src/d3d9fe/*.cpp)

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

    # wsi_platform only: the win32 driver files are Win32 throughout, and d9mt
    # installs its own driver over the bootstrap entry supplied by
    # d3d9fe/d9mt_wsi_bootstrap.cpp.
    ${V}/src/wsi/wsi_platform.cpp

    # Enum name tables; the loader itself is stubbed in d3d9fe/stubs.cpp.
    ${V}/src/vulkan/vulkan_names.cpp

    # SPIR-V to MSL, verbatim Khronos, carried by d9mt.
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
    ${D9}/src/d3d9fe
    # winemetal.h lives beside d9mt's other top-level headers; the ABI it
    # declares is implemented by src/MetalBridge, not fetched.
    ${D9}/src
    # DXVK generates these from GLSL with meson at build time; d9mt carries
    # them pre-generated, which is why meson is not needed here.
    ${V}/generated
    ${SC}
)

# DXVK selects a window-system driver from a table guarded by these. d9mt
# repoints the Win32 entry at its own factory rather than editing the tree, so
# the define stays on and the entry comes from d9mt_wsi_bootstrap.cpp.
target_compile_definitions(${PROJECT_NAME} PRIVATE DXVK_WSI_WIN32)

# d9mt's frontend trace writes a line per D3D9 call and fflushes each one. It is
# a debugging instrument and it dominates everything: left on, it produced a
# 2.3 GB log in under a minute and spent about 90% of frame time inside
# write(). This compiles every FETRACE call out. Turn it back on by removing
# this line, not by editing extern/.
target_compile_definitions(${PROJECT_NAME} PRIVATE D9MT_NO_FETRACE)

target_link_libraries(${PROJECT_NAME} PUBLIC XPlatform MetalBridge)

# Both trees expect <windows.h> to have been seen already.
target_compile_options(${PROJECT_NAME} PRIVATE -include xplatform.h)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)

# Third-party: upstream's warnings are not this project's to fix, and there are
# thousands of them.
target_compile_options(${PROJECT_NAME} PRIVATE -w)
