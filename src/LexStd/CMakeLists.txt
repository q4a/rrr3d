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

target_include_directories(${PROJECT_NAME} PRIVATE
    header
)

# Built from source now rather than linked as a prebuilt .lib; see
# tools/vendor-tinyxml.py. PUBLIC so the TIXML_USE_STL that comes with it
# reaches anything that ends up including tinyxml.h through LexStd.
target_link_libraries(${PROJECT_NAME} PUBLIC TinyXml)

if (NOT MSVC)
    target_link_libraries(${PROJECT_NAME} PUBLIC XPlatform)
endif()
