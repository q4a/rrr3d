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

if(WIN32)
    add_library(${PROJECT_NAME} SHARED
        ${${PROJECT_NAME}_SOURCE}
        ${${PROJECT_NAME}_INLINE}
        ${${PROJECT_NAME}_HEADER}
    )
else()
    # Keep the native transport inside the application bundle.  Windows
    # retains the original DLL boundary and NETLIB_API exports.
    add_library(${PROJECT_NAME} STATIC
        ${${PROJECT_NAME}_SOURCE}
        ${${PROJECT_NAME}_INLINE}
        ${${PROJECT_NAME}_HEADER}
    )
endif()

target_include_directories(${PROJECT_NAME}
    PUBLIC
    include
    PRIVATE
    header
    ${CMAKE_SOURCE_DIR}/src/LexStd/header
    ${CMAKE_SOURCE_DIR}/src/MathLib/header
    ${CMAKE_SOURCE_DIR}/src/XPlatform/header
)

if (WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/boost/include
        ${CMAKE_SOURCE_DIR}/extern/directx/include
    )
    set(SYSTEM_DEPS "Iphlpapi.lib")
endif()

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    MathLib
    PRIVATE
    ${RRR3D_BOOST_TARGET}
    ${SYSTEM_DEPS}
)

target_link_directories(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/extern/boost/lib/release>"
    "$<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/extern/boost/lib/debug>"
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    -DNETLIB_EXPORTS
)

rrr3d_set_common_target_options(${PROJECT_NAME})

if(NOT WIN32)
    add_executable(rrr3d_net_loopback_smoke
        tests/NetLoopbackSmoke.cpp
    )
    target_include_directories(rrr3d_net_loopback_smoke PRIVATE
        header
        include
    )
    target_link_libraries(rrr3d_net_loopback_smoke PRIVATE
        NetLib
        ${RRR3D_BOOST_TARGET}
    )
    rrr3d_set_common_target_options(rrr3d_net_loopback_smoke)
    add_test(NAME rrr3d_net_loopback_smoke COMMAND rrr3d_net_loopback_smoke)
endif()
