project(Tests)

add_definitions(-DNOMINMAX) # Use std::max()/std::min()

list(APPEND PROJECT_DIRS
    "source"
    )
add_dir("${PROJECT_DIRS}" "${PROJECT_NAME}")

add_executable(${PROJECT_NAME}
    ${${PROJECT_NAME}_SOURCE}
    ${${PROJECT_NAME}_HEADER}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    MathLib
)

if (NOT WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE XPlatform)
endif()

add_test(NAME port-tests COMMAND ${PROJECT_NAME})
