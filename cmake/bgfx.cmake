if(NOT APPLE)
    message(FATAL_ERROR "The pinned bgfx integration currently targets macOS only")
endif()

set(RRR3D_BGFX_SOURCE_DIR "${RRR3D_BGFX_ROOT}/bgfx")
set(RRR3D_BX_SOURCE_DIR "${RRR3D_BGFX_ROOT}/bx")
set(RRR3D_BIMG_SOURCE_DIR "${RRR3D_BGFX_ROOT}/bimg")
set(_rrr3d_bgfx_bin "${RRR3D_BGFX_SOURCE_DIR}/.build/osx-arm64/bin")

set(_rrr3d_bgfx_library "${_rrr3d_bgfx_bin}/libbgfxRelease.a")
set(_rrr3d_bx_library "${_rrr3d_bgfx_bin}/libbxRelease.a")
set(_rrr3d_bimg_library "${_rrr3d_bgfx_bin}/libbimgRelease.a")
set(_rrr3d_bimg_decode_library
    "${_rrr3d_bgfx_bin}/libbimg_decodeRelease.a")
set(RRR3D_BGFX_SHADERC "${_rrr3d_bgfx_bin}/shadercRelease")

foreach(_required_path IN ITEMS
        "${RRR3D_BGFX_SOURCE_DIR}/include/bgfx/bgfx.h"
        "${RRR3D_BX_SOURCE_DIR}/include/bx/math.h"
        "${RRR3D_BIMG_SOURCE_DIR}/include/bimg/bimg.h"
        "${_rrr3d_bgfx_library}"
        "${_rrr3d_bx_library}"
        "${_rrr3d_bimg_library}"
        "${_rrr3d_bimg_decode_library}"
        "${RRR3D_BGFX_SHADERC}")
    if(NOT EXISTS "${_required_path}")
        message(FATAL_ERROR
            "Pinned bgfx artifact is missing: ${_required_path}\n"
            "Run scripts/build_bgfx_macos.sh before configuring Milestone 5 or 6.")
    endif()
endforeach()

add_library(rrr3d_bx STATIC IMPORTED GLOBAL)
set_target_properties(rrr3d_bx PROPERTIES
    IMPORTED_LOCATION "${_rrr3d_bx_library}"
    INTERFACE_INCLUDE_DIRECTORIES "${RRR3D_BX_SOURCE_DIR}/include"
    INTERFACE_COMPILE_DEFINITIONS "BX_CONFIG_DEBUG=0"
    INTERFACE_COMPILE_FEATURES "cxx_std_20"
)

add_library(rrr3d_bimg STATIC IMPORTED GLOBAL)
set_target_properties(rrr3d_bimg PROPERTIES
    IMPORTED_LOCATION "${_rrr3d_bimg_library}"
    INTERFACE_INCLUDE_DIRECTORIES "${RRR3D_BIMG_SOURCE_DIR}/include"
)
target_link_libraries(rrr3d_bimg INTERFACE rrr3d_bx)

add_library(rrr3d_bimg_decode STATIC IMPORTED GLOBAL)
set_target_properties(rrr3d_bimg_decode PROPERTIES
    IMPORTED_LOCATION "${_rrr3d_bimg_decode_library}"
    INTERFACE_INCLUDE_DIRECTORIES "${RRR3D_BIMG_SOURCE_DIR}/include"
)
target_link_libraries(rrr3d_bimg_decode INTERFACE
    rrr3d_bimg
    rrr3d_bx
)

find_library(RRR3D_COCOA_FRAMEWORK Cocoa REQUIRED)
find_library(RRR3D_METAL_FRAMEWORK Metal REQUIRED)
find_library(RRR3D_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(RRR3D_IOKIT_FRAMEWORK IOKit REQUIRED)
find_library(RRR3D_VIDEOTOOLBOX_FRAMEWORK VideoToolbox REQUIRED)

add_library(rrr3d_bgfx STATIC IMPORTED GLOBAL)
set_target_properties(rrr3d_bgfx PROPERTIES
    IMPORTED_LOCATION "${_rrr3d_bgfx_library}"
    INTERFACE_INCLUDE_DIRECTORIES "${RRR3D_BGFX_SOURCE_DIR}/include"
)
target_link_libraries(rrr3d_bgfx INTERFACE
    rrr3d_bimg_decode
    rrr3d_bimg
    rrr3d_bx
    "${RRR3D_COCOA_FRAMEWORK}"
    "${RRR3D_METAL_FRAMEWORK}"
    "${RRR3D_QUARTZCORE_FRAMEWORK}"
    "${RRR3D_IOKIT_FRAMEWORK}"
    "${RRR3D_VIDEOTOOLBOX_FRAMEWORK}"
)
