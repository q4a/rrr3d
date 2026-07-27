project(LexStd)

add_definitions(-DNOMINMAX) # Use std::max()/std::min()

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

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/extern/tinyxml/include
)

if (NOT WIN32)
    # Substitutes for <windows.h> and the Win32 calls this library uses
    target_link_libraries(${PROJECT_NAME} PUBLIC XPlatform)
endif()
