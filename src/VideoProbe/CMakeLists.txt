# The cutscene pipeline, without the game.
#
# Demuxes an .avi and decodes H.264 through VideoToolbox, then writes a frame as
# a TGA and checks it is not a single flat colour. Nothing of the engine is
# involved, so when a cutscene is black in game this says whether the fault is
# the decode or how it reaches the screen -- the same role src/D3D9Triangle
# plays for the rendering stack.
project(VideoProbe)

add_executable(${PROJECT_NAME}
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/Rock3dGame/source/video/avi_reader.cpp
    ${CMAKE_SOURCE_DIR}/src/Rock3dGame/source/video/h264_poc.cpp
    ${CMAKE_SOURCE_DIR}/src/Rock3dGame/source/video/h264_decoder.mm
)

# The two sources above are compiled here rather than linked from Rock3dGame,
# because linking Rock3dGame would pull in the engine, the D3D9 backend and the
# whole game -- which is exactly what this is for not needing.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Rock3dGame/header
    ${CMAKE_SOURCE_DIR}/src/Rock3dGame/include
    ${CMAKE_SOURCE_DIR}/src/LexStd/header
    ${CMAKE_SOURCE_DIR}/src/MathLib/header
)

target_link_libraries(${PROJECT_NAME} PRIVATE XPlatform)

target_link_libraries(${PROJECT_NAME} PRIVATE
    "-framework CoreMedia"
    "-framework CoreVideo"
    "-framework VideoToolbox"
    "-framework CoreFoundation"
)
