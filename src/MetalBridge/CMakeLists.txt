project(MetalBridge)

# Native implementation of the winemetal ABI -- see header/metalbridge.h.
# No Wine involved: the ABI exists to cross Wine's wow64 boundary, and this
# build has no boundary to cross, so the same calls are made directly.

add_library(${PROJECT_NAME} STATIC
    source/metalbridge.mm
    header/metalbridge.h
)

target_include_directories(${PROJECT_NAME} PUBLIC header)

# ARC off: the ABI's contract is manual retain/release, and the handles are
# unowned bridged pointers. Letting ARC manage them would double-free.
set_source_files_properties(source/metalbridge.mm PROPERTIES
    COMPILE_FLAGS "-fno-objc-arc"
)

find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC
    ${METAL_FRAMEWORK}
    ${FOUNDATION_FRAMEWORK}
    ${QUARTZCORE_FRAMEWORK}
)
