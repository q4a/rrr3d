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

target_include_directories(${PROJECT_NAME} PUBLIC
    header
    header/d3d
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/LexStd/header
)

if (NOT WIN32)
    # d3dx9_compat.h defers to xplatform.h for the Windows scalar types
    target_link_libraries(${PROJECT_NAME} PUBLIC XPlatform)
endif()

if (MSVC)
    # d3d9types.h, for the D3D base types the compat header defers to on Windows
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/directx/include
    )
endif()

target_link_libraries(${PROJECT_NAME})
