project(XPlatform)

list(APPEND PROJECT_DIRS
    "header"
    "source"
    )
add_dir("${PROJECT_DIRS}" "${PROJECT_NAME}")

add_library(${PROJECT_NAME} STATIC
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
