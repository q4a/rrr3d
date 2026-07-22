project(Rock3dEngine)

if(NOT WIN32 AND RRR3D_ENABLE_RENDERER AND
   NOT RRR3D_RENDERER_BACKEND STREQUAL "bgfx")
    message(FATAL_ERROR
        "The non-Windows renderer requires RRR3D_RENDERER_BACKEND=bgfx")
endif()

if(WIN32)
    list(APPEND PROJECT_DIRS
        "header"
        "header/graph"
        "header/graph/Driver"
        "header/gui"
        "header/include"
        "header/include/graph"
        "header/px"
        "header/resource"
        "header/res"
        "source"
        "source/graph"
        "source/graph/Driver"
        "source/gui"
        "source/px"
        "source/resource"
        "source/res"
    )
    add_dir("${PROJECT_DIRS}" "${PROJECT_NAME}")

    list(REMOVE_ITEM ${PROJECT_NAME}_HEADER
        "${CMAKE_CURRENT_SOURCE_DIR}/header/graph/GaussianBlur.h"
    )
    list(REMOVE_ITEM ${PROJECT_NAME}_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/source/graph/GaussianBlur.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/res/R3DFile.cpp"
    )
else()
    # Portable core that does not manufacture fake D3D or PhysX objects.
    # Renderer/physics entry points remain explicitly unavailable until their
    # own milestones provide real implementations.
    set(${PROJECT_NAME}_SOURCE
        source/ProgressTimer.cpp
        source/resource/R3DMeshAsset.cpp
        source/resource/ResourceFileSystem.cpp
        source/stub/PortableEngine.cpp
    )
    set(${PROJECT_NAME}_HEADER
        header/ProgressTimer.h
        header/PortableEngine.h
        header/audio/AudioBackend.h
        header/resource/R3DMeshAsset.h
        header/resource/ResourceFileSystem.h
        header/stdafx.h
    )

    if(RRR3D_ENABLE_RENDERER AND RRR3D_RENDERER_BACKEND STREQUAL "bgfx")
        list(APPEND ${PROJECT_NAME}_SOURCE
            source/renderer/bgfx/BgfxGraphicsDevice.cpp
        )
        list(APPEND ${PROJECT_NAME}_HEADER
            header/renderer/Renderer.h
            header/renderer/BgfxGraphicsDevice.h
        )
    endif()

    if(RRR3D_ENABLE_PHYSICS)
        list(APPEND ${PROJECT_NAME}_SOURCE
            source/physics/MinimalVehiclePhysics.cpp
        )
        list(APPEND ${PROJECT_NAME}_HEADER
            header/physics/PhysicsBackend.h
        )
    endif()
endif()

add_library(${PROJECT_NAME} STATIC
    ${${PROJECT_NAME}_SOURCE}
    ${${PROJECT_NAME}_INLINE}
    ${${PROJECT_NAME}_HEADER}
)

target_include_directories(${PROJECT_NAME} PUBLIC
    header
)

if(WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/physx/include/Cooking
        ${CMAKE_SOURCE_DIR}/extern/physx/include/Foundation
        ${CMAKE_SOURCE_DIR}/extern/physx/include/Physics
        ${CMAKE_SOURCE_DIR}/extern/physx/include/PhysXLoader
        ${CMAKE_SOURCE_DIR}/extern/directx/include
    )
else()
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        RRR3D_PORTABLE_ENGINE_STUBS=1
    )

    if(RRR3D_ENABLE_RENDERER AND RRR3D_RENDERER_BACKEND STREQUAL "bgfx")
        target_compile_definitions(${PROJECT_NAME} PUBLIC
            RRR3D_RENDERER_BGFX=1
        )
        target_link_libraries(${PROJECT_NAME} PUBLIC rrr3d_bgfx)
    endif()

    if(RRR3D_ENABLE_PHYSICS)
        target_compile_definitions(${PROJECT_NAME} PUBLIC
            RRR3D_PHYSICS_MINIMAL=1
        )
    endif()
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC
    LexStd
    MathLib
    XPlatform
)

rrr3d_set_common_target_options(${PROJECT_NAME})
