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
)

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
)
target_link_libraries(PhysX28Tests PRIVATE ${PROJECT_NAME})
