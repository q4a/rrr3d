# Does Dear ImGui's D3D9 backend draw on this stack?
#
# imgui_impl_dx9 uses fixed-function TnL, and the engine binds a shader for
# every draw it makes -- so DXVK's fixed-function generator has almost
# certainly never run in this build. This answers that in one command, before
# the map editor is built on the assumption that it works.
#
# Checks pixels and exits non-zero, the way src/D3D9Triangle does, because the
# failure mode here is a skipped draw that still returns S_OK.
project(D3D9ImGui)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE XPlatform D3D9Metal ImGuiLib)
