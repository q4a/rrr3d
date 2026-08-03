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
    # Boost's MSVC auto-linking, off.
    #
    # Boost headers emit `#pragma comment(lib, ...)` naming a compiled library
    # for the exact toolset and version they were included from -- so pinning
    # the headers to 1.69 (tools/setup-boost.py) made MSVC demand
    # libboost_date_time-vc141-mt-gd-x32-1_69.lib, which nothing provides:
    # extern.7z carries 1.59 binaries. Asio pulls date_time in for
    # deadline_timer, so it happens even though nothing here uses date_time
    # directly.
    #
    # Nothing to link, and that is the point of the pin: NetLib uses Asio, bind
    # and system, all header-only in 1.69. This says so rather than shipping a
    # binary to satisfy a pragma. Off Windows the pragma does not exist and the
    # definition is harmless, so it is not conditional.
    -DBOOST_ALL_NO_LIB
)
