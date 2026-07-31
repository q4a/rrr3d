project(MathLib)

add_definitions(-DNOMINMAX) # Use std::max()/std::min()

list(APPEND PROJECT_DIRS
    "header"
    "header/d3d"
    "source"
    )
add_dir("${PROJECT_DIRS}" "${PROJECT_NAME}")

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
