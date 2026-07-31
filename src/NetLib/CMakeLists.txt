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
    ${CMAKE_SOURCE_DIR}/src/MathLib/header
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    LexStd
)

# One pinned Boost for both platforms, fetched by tools/setup-boost.py. The
# bundled 1.59 predates C++17 and uses std::auto_ptr; a current Boost has
# removed io_service and get_io_service(), both of which NetLib uses. 1.69 is
# the version that satisfies both, so NetLib compiles unchanged. Headers only:
# Asio, bind and system are all header-only there.
set(BOOST_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/extern/boost169/include)
if (NOT EXISTS ${BOOST_INCLUDE_DIR}/boost/version.hpp)
    message(FATAL_ERROR
        "Boost headers not found at ${BOOST_INCLUDE_DIR}. Run tools/setup-boost.py")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${BOOST_INCLUDE_DIR}
)

if (MSVC)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/directx/include
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE Iphlpapi.lib)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE XPlatform)
endif()

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    -DNETLIB_EXPORTS
)
