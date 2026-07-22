project(XPlatform)

add_library(${PROJECT_NAME} STATIC
    source/xplatform.cpp
    header/xplatform.h
    header/d3dx9math.h
)

target_include_directories(${PROJECT_NAME} PUBLIC
    header
)

if(CMAKE_DL_LIBS)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
endif()

rrr3d_set_common_target_options(${PROJECT_NAME})
