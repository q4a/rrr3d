project(MathLib)

add_definitions(-DNOMINMAX) # Use std::max()/std::min()

# header/d3d is gone -- the vendored d3dx9math.{h,inl} it held were removed in
# d3d6c21, and MathCommon.h now resolves <d3dx9math.h> against the SDK on
# Windows and XPlatform's vendored DirectX headers elsewhere.
list(APPEND PROJECT_DIRS
    "header"
    "source"
    )
add_dir("${PROJECT_DIRS}" "${PROJECT_NAME}")

# Wine's D3DX math implementation, vendored by tools/vendor-wine-d3dx9math.py.
#
# Windows must NOT compile it. The MSVC targets link d3dx9.lib, which exports
# every one of the ~100 WINAPI D3DX* functions this file defines, and the glob
# above would otherwise hand the linker both -- LNK2005 on each. Off Windows it
# is the only implementation there is.
if (MSVC)
    list(REMOVE_ITEM ${PROJECT_NAME}_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/source/d3dx9math.c"
    )
endif()

add_library(${PROJECT_NAME} STATIC
    ${${PROJECT_NAME}_SOURCE}
    ${${PROJECT_NAME}_INLINE}
    ${${PROJECT_NAME}_HEADER}
)

# PUBLIC: d3d/d3dx9math.h is the D3DX math surface for the whole engine, and
# MathCommon.h -- which every consumer includes -- pulls it in from here.
target_include_directories(${PROJECT_NAME} PUBLIC
    header
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/LexStd/header
)

if (MSVC)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/directx/include
    )
else()
    # XPlatform carries d3d9types.h, which d3dx9math.h needs for the base types
    # its structs derive from.
    target_link_libraries(${PROJECT_NAME} PUBLIC XPlatform)
endif()
