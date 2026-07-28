# A triangle through the winemetal ABI alone -- see bridge_triangle.cpp.
#
# src/D3D9Triangle tests the whole stack. This tests only src/MetalBridge, so
# between them they say which side of the bridge a rendering fault is on.
project(BridgeTriangle)

add_executable(${PROJECT_NAME} bridge_triangle.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE XPlatform MetalBridge)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../header
)

# The shader is compiled ahead of time rather than through d9mt's runtime
# compiler, so that a shader-compilation fault cannot be mistaken for an
# encoding one.
set(TRI_OUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug)

foreach(shader tri tri_stagein tri_argbuf tri_speccnst tri_blend)
    add_custom_command(
        OUTPUT ${TRI_OUT}/${shader}.metallib
        COMMAND ${CMAKE_COMMAND} -E make_directory ${TRI_OUT}
        # -std=metal3.0 matches what d9mtmetal compiles game shaders with;
        # argument-buffer addressing behaves differently under other versions.
        COMMAND xcrun -sdk macosx metal -std=metal3.0 -ffast-math
                -o ${TRI_OUT}/${shader}.metallib
                ${CMAKE_CURRENT_SOURCE_DIR}/${shader}.metal
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${shader}.metal
        COMMENT "Compiling ${shader}.metal"
        VERBATIM
    )
    list(APPEND TRI_METALLIBS ${TRI_OUT}/${shader}.metallib)
endforeach()

add_custom_target(${PROJECT_NAME}_shader DEPENDS ${TRI_METALLIBS})
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_shader)
