project(PhysX28)

# The PhysX 2.8 compatibility shim.
#
# The game is written against PhysX 2.8, whose SDK is a closed 32-bit Windows
# binary that cannot be built for anything else -- and whose *headers* do not
# even compile here (Nx.h stops with "Platforms pointer size ambiguous",
# NxMath.h wants a Linux variant that does not ship). So the 2.8 API becomes
# ours to implement.
#
# The rule, and the reason this is a shim rather than a migration: the game must
# not be able to tell. No game constant, no db.xml value, no tuning parameter is
# adjusted to suit the backend. Where 2.8 defines a behaviour, this implements
# that behaviour.
#
# Backend: Bullet, chosen by the trial in docs/physics-backend-trial.md -- the
# only candidate with no capability gap against 2.8.

add_library(${PROJECT_NAME} STATIC
    source/Px28Enums.cpp
    source/Px28Impl.cpp
    source/Px28Scene.cpp
    source/Px28Actor.cpp
    source/Px28Shape.cpp
    source/Px28Wheel.cpp
    source/Px28Contact.cpp
    source/Px28Mesh.cpp
)

# Bullet, and this is the only target in the tree that names it. It stays
# PRIVATE so no Bullet header, type or macro can reach a consumer -- the whole
# point of the abstract interfaces in include/ is that Rock3dGame's 52
# translation units never see a backend.
if (MSVC)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/bullet/include
    )
    set(BULLET_LIBRARIES
        "$<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/extern/bullet/lib${LIBSUFFIX}/release/BulletCollision.lib>"
        "$<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/extern/bullet/lib${LIBSUFFIX}/release/BulletDynamics.lib>"
        "$<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/extern/bullet/lib${LIBSUFFIX}/release/LinearMath.lib>"
        "$<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/extern/bullet/lib${LIBSUFFIX}/debug/BulletCollision_Debug.lib>"
        "$<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/extern/bullet/lib${LIBSUFFIX}/debug/BulletDynamics_Debug.lib>"
        "$<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/extern/bullet/lib${LIBSUFFIX}/debug/LinearMath_Debug.lib>"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE ${BULLET_LIBRARIES})
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(BULLET REQUIRED IMPORTED_TARGET bullet)
    target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::BULLET)
endif()

# PUBLIC: consumers include <NxPhysics.h> and friends by those names, exactly as
# they do against the real SDK.
target_include_directories(${PROJECT_NAME} PUBLIC
    include
)

# --- tests -------------------------------------------------------------------
#
# Deliberately dependency-free: no backend, no simulation, no window. The values
# these check are serialised into shipped game data, so they can be wrong
# silently, and checking them costs nothing.

add_executable(PhysX28Tests
    test/Px28Tests.cpp
    test/Px28Enums.cpp
    test/Px28Vec3.cpp
    test/Px28Quat.cpp
    test/Px28Mat.cpp
    test/Px28Desc.cpp
)
target_link_libraries(PhysX28Tests PRIVATE ${PROJECT_NAME})

# --- harness -----------------------------------------------------------------
#
# Scenarios that need the backend, which is why they are not in PhysX28Tests.
# Still no Rock3dEngine and no windows.h, so this runs long before the graphics
# port does and fails on its own terms.

add_executable(PhysX28Harness
    harness/Px28Harness.cpp
)
if(MSVC)
    target_include_directories(PhysX28Harness PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/bullet/include
    )
    target_link_libraries(PhysX28Harness PRIVATE ${PROJECT_NAME} ${BULLET_LIBRARIES})
else()
    target_link_libraries(PhysX28Harness PRIVATE ${PROJECT_NAME} PkgConfig::BULLET)
endif()
