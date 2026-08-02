# Dear ImGui, fetched by tools/setup-imgui.py into extern/imgui.
#
# A static library rather than sources compiled into each consumer: there will
# be two of them -- the smoke test and the map editor -- and ImGui keeps global
# state in its context, so one copy is not merely tidier, it is the correct
# number.
#
# The docking branch, because the editor's layout is dockable panes around a
# viewport. Multi-viewport is deliberately unused; see the setup script.
project(ImGuiLib)

set(IMGUI ${CMAKE_SOURCE_DIR}/extern/imgui)

add_library(${PROJECT_NAME} STATIC
    ${IMGUI}/imgui.cpp
    ${IMGUI}/imgui_draw.cpp
    ${IMGUI}/imgui_tables.cpp
    ${IMGUI}/imgui_widgets.cpp
    ${IMGUI}/imgui_impl_sdl3.cpp
    ${IMGUI}/imgui_impl_dx9.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${IMGUI})

# XPlatform for d3d9.h, which imgui_impl_dx9.cpp includes by that bare name
# exactly as it does on Windows. PUBLIC so consumers get it too.
target_link_libraries(${PROJECT_NAME} PUBLIC XPlatform)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
target_link_libraries(${PROJECT_NAME} PUBLIC PkgConfig::SDL3)

# Vendored third-party source, held to its own standards rather than ours.
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wno-unused-parameter
    -Wno-unused-variable
)
