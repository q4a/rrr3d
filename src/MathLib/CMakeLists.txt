project(MathLib)

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
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
    NOMINMAX
    GLM_ENABLE_EXPERIMENTAL
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    LexStd
    XPlatform
    ${RRR3D_GLM_TARGET}
)

rrr3d_set_common_target_options(${PROJECT_NAME})
