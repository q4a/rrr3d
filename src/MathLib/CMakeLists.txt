project(MathLib)

if (WIN32)
set(DX9MATH_PATH
    ${CMAKE_SOURCE_DIR}/extern/directx/include
)
else()
set(DX9MATH_PATH
    ${CMAKE_SOURCE_DIR}/src/XPlatform/header
    ${DXVK_NATIVE_INCLUDE_DIRS}
)
endif()

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

target_include_directories(${PROJECT_NAME} PRIVATE
    header
    ${CMAKE_SOURCE_DIR}/src/LexStd/header
    ${CMAKE_SOURCE_DIR}/extern/glm/include
    ${DX9MATH_PATH}
)

target_link_libraries(${PROJECT_NAME})
