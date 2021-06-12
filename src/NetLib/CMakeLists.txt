project(NetLib)

add_definitions(-DNOMINMAX) # Use std::max()/std::min()

list(APPEND PROJECT_DIRS
    "header"
    "header/net"
    "include/net"
    "source"
    "source/net"
    )
add_dir("${PROJECT_DIRS}" "${PROJECT_NAME}")

add_library(${PROJECT_NAME} SHARED
    ${${PROJECT_NAME}_SOURCE}
    ${${PROJECT_NAME}_INLINE}
    ${${PROJECT_NAME}_HEADER}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    header
    include
    ${CMAKE_SOURCE_DIR}/src/LexStd/header
    ${CMAKE_SOURCE_DIR}/extern/boost/include
    ${CMAKE_SOURCE_DIR}/extern/directx/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    LexStd
    Iphlpapi.lib
)

target_link_directories(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/extern/boost/lib/release>"
    "$<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/extern/boost/lib/debug>"
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    -DNETLIB_EXPORTS
)
