project(MapEditor)

# Two editors, one name.
#
# MSVC builds the MFC one, unchanged. Everything else builds the Dear ImGui one,
# which is not a port of it -- the editor's logic was always in the engine's
# r3d::edit::* interfaces, and MFC only held the tree-control bookkeeping that
# immediate mode does not need. See editor_main.cpp.
#
# The MFC sources stay rather than being deleted: nothing here removes a working
# thing, and whether Windows keeps a second editor is a decision for whoever
# builds it there.
if (MSVC)


list(APPEND PROJECT_DIRS
    "."
    )
add_dir("${PROJECT_DIRS}" "${PROJECT_NAME}")

# The ImGui editor's entry point lives in this directory too, and add_dir globs
# the whole directory -- so MSVC compiled it and stopped on a missing imgui.h.
# The MFC editor is these sources minus that one.
#
# FILTER on a regex rather than REMOVE_ITEM on a path, and that is not a style
# choice. add_dir is called here with "." as the directory, so its
# file(GLOB "./*.cpp") returns paths of the form
# .../src/MapEditor/./editor_main.cpp -- with the "./" still in them. An exact
# REMOVE_ITEM never matches that and removes nothing, silently. The other
# exclusions in this tree happen to work only because their projects name real
# subdirectories, so no "./" appears.
list(FILTER ${PROJECT_NAME}_SOURCE EXCLUDE REGEX "editor_main\\.cpp$")

add_definitions(-D_AFXDLL)
set(CMAKE_MFC_FLAG 2)

add_executable(${PROJECT_NAME} WIN32
    ${${PROJECT_NAME}_SOURCE}
    ${${PROJECT_NAME}_INLINE}
    ${${PROJECT_NAME}_HEADER}
    "res/MapEditor.rc2"
    "MapEditor.reg"
    "MapEditor.rc"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/LexStd/header
    ${CMAKE_SOURCE_DIR}/src/MathLib/header
    ${CMAKE_SOURCE_DIR}/src/Rock3dGame/include
    ${CMAKE_SOURCE_DIR}/extern/directx/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    LexStd
    MathLib
    Rock3dGame
    ${CMAKE_SOURCE_DIR}/extern/directx/lib/d3dx9.lib
)

else()

    # The ImGui editor. Only if ImGui was fetched -- the same rule the D3D9
    # backend gets, so a tree without extern/ still builds and tests.
    if (TARGET ImGuiLib)
        add_executable(${PROJECT_NAME} editor_main.cpp)

        target_include_directories(${PROJECT_NAME} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/LexStd/header
            ${CMAKE_SOURCE_DIR}/src/MathLib/header
            ${CMAKE_SOURCE_DIR}/src/Rock3dGame/include
        )

        target_link_libraries(${PROJECT_NAME} PRIVATE
            LexStd
            MathLib
            Rock3dGame
            ImGuiLib
        )

        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
        target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::SDL3)
    else()
        message(STATUS "MapEditor: needs extern/imgui -- run tools/setup-imgui.py")
    endif()

endif()

# Retina drawables need NSHighResolutionCapable; see cmake/utils.cmake.
# Guarded because this target does not exist when its dependencies are absent.
if (TARGET ${PROJECT_NAME})
    rrr3d_high_dpi(${PROJECT_NAME})
endif()
