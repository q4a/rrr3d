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

function(rrr3d_set_common_target_options target_name)
  target_compile_features(${target_name} PUBLIC cxx_std_17)
  target_compile_definitions(${target_name} PUBLIC
    "$<$<CONFIG:Debug>:_DEBUG>"
  )

  if(MSVC)
    target_compile_options(${target_name} PRIVATE /W3)
  else()
    target_compile_options(${target_name} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wno-unused-parameter
      # The original sources contain CP1251 comments. Converting every comment
      # is a separate mechanical cleanup and does not affect compiled strings.
      -Wno-invalid-utf8
    )
  endif()
endfunction()
