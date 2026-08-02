project(Tests)

# Runtime checks for the substrate the rest of the port stands on: XPlatform's
# Win32 semantics and MathLib's D3DX math.
#
# Non-MSVC only, and that is not an oversight. On Windows these are the real
# Win32 and the real D3DX, so the checks would be testing Microsoft's code --
# which is not the thing at risk. What is at risk is the substitutes.

add_executable(${PROJECT_NAME}
    Tests.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    MathLib
    XPlatform
)

# LexStd's headers, for the string helpers. Headers only -- lslUtility.h is
# entirely inline, so there is nothing to link and no dependency on the rest of
# LexStd comes with it.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/LexStd/header
)
