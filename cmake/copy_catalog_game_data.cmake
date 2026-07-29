if(NOT DEFINED source_root OR NOT DEFINED destination_root)
    message(FATAL_ERROR
        "copy_catalog_game_data requires source_root and destination_root")
endif()

set(catalog "${source_root}/legacy-assets.catalog")
if(NOT EXISTS "${catalog}")
    message(FATAL_ERROR "game-data catalog is missing: ${catalog}")
endif()

file(REMOVE_RECURSE "${destination_root}")
file(MAKE_DIRECTORY "${destination_root}")
file(STRINGS "${catalog}" catalog_entries ENCODING UTF-8)

set(resource_paths)
foreach(entry IN LISTS catalog_entries)
    string(REGEX REPLACE "^[^	]*	" "" relative_path "${entry}")
    if(relative_path STREQUAL entry OR
       IS_ABSOLUTE "${relative_path}" OR
       relative_path MATCHES "(^|/)\\.\\.(/|$)")
        message(FATAL_ERROR "invalid game-data catalog entry: ${entry}")
    endif()
    list(APPEND resource_paths "${relative_path}")
endforeach()

list(APPEND resource_paths
    "legacy-assets.catalog"
    "manifest.cfg"
    "menu/menu.cfg"
    "ui/font5x7.txt")

foreach(relative_path IN LISTS resource_paths)
    set(source "${source_root}/${relative_path}")
    set(destination "${destination_root}/${relative_path}")
    if(NOT EXISTS "${source}")
        message(FATAL_ERROR "cataloged game resource is missing: ${source}")
    endif()
    get_filename_component(destination_directory "${destination}" DIRECTORY)
    file(MAKE_DIRECTORY "${destination_directory}")
    file(COPY_FILE "${source}" "${destination}" ONLY_IF_DIFFERENT)
endforeach()

list(LENGTH resource_paths copied_count)
message(STATUS "Copied ${copied_count} cataloged game-data files")
