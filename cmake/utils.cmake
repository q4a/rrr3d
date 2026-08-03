macro(add_dir DIRS FILE_GROUP)
  foreach(dir ${DIRS})
    message("adding ${dir} to ${FILE_GROUP}")
#    include_directories(${dir})
    file( GLOB ${dir}_SOURCE_ADD ${dir}/*.cpp ${dir}/*.cxx ${dir}/*.c )
    list( APPEND ${FILE_GROUP}_SOURCE ${${dir}_SOURCE_ADD} )
    file( GLOB ${dir}_INLINE_ADD ${dir}/*.inl )
    list( APPEND ${FILE_GROUP}_INLINE ${${dir}_INLINE_ADD} )
    file( GLOB ${dir}_HEADER_ADD ${dir}/*.h ${dir}/*.hpp )
    list( APPEND ${FILE_GROUP}_HEADER ${${dir}_HEADER_ADD} )
  endforeach()
endmacro()

# Declare an executable high-DPI capable on macOS, without an .app bundle.
#
# The system reads NSHighResolutionCapable from a bundle's Info.plist, or -- for
# a bare executable -- from a __TEXT,__info_plist section the linker embeds.
# Without it a window never gets a Retina drawable however it is created, and
# the whole application is rendered at half resolution and upscaled.
#
# A bundle is the usual answer and is not cheap here: lsl::GetAppPath returns
# the executable's own directory and every asset is resolved against it, so
# moving the executable into Contents/MacOS moves the entire Data tree or
# teaches GetAppPath about bundles. See cmake/macos-info.plist.
function(rrr3d_high_dpi target)
    if (NOT APPLE)
        return()
    endif()

    target_link_options(${target} PRIVATE
        "LINKER:-sectcreate,__TEXT,__info_plist,${CMAKE_SOURCE_DIR}/cmake/macos-info.plist")

    # Relink when the plist changes, which the linker flag alone does not do.
    set_property(TARGET ${target} APPEND PROPERTY
        LINK_DEPENDS ${CMAKE_SOURCE_DIR}/cmake/macos-info.plist)
endfunction()
