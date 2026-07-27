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
    $<$<BOOL:${MSVC}>:${CMAKE_SOURCE_DIR}/extern/tinyxml/include>
)

if (NOT WIN32)
    # Substitutes for <windows.h> and the Win32 calls this library uses
    target_link_libraries(${PROJECT_NAME} PUBLIC XPlatform)
endif()

if (NOT WIN32)
    # Vendored TinyXML 2.5.3; carries TIXML_USE_STL and its own include path.
    target_link_libraries(${PROJECT_NAME} PUBLIC TinyXml)
endif()
