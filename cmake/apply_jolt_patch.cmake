if(NOT DEFINED SOURCE_DIR OR NOT DEFINED PATCH_FILE)
    message(FATAL_ERROR "SOURCE_DIR and PATCH_FILE are required")
endif()

set(_vehicle_header
    "${SOURCE_DIR}/Jolt/Physics/Vehicle/VehicleConstraint.h")
if(NOT EXISTS "${_vehicle_header}")
    message(FATAL_ERROR "Jolt source tree is incomplete: ${_vehicle_header}")
endif()

file(READ "${_vehicle_header}" _vehicle_header_contents)
string(FIND "${_vehicle_header_contents}"
    "SuspensionMaxImpulseCallback" _already_patched)
if(NOT _already_patched EQUAL -1)
    message(STATUS "Jolt source wheel normal-force patch already applied")
    return()
endif()

execute_process(
    COMMAND patch -p1 -i "${PATCH_FILE}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE _patch_result
)
if(NOT _patch_result EQUAL 0)
    message(FATAL_ERROR "Could not apply ${PATCH_FILE} to pinned Jolt")
endif()
