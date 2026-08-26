if(NOT DEFINED SOURCE_DIR OR NOT DEFINED PATCH_FILE)
    message(FATAL_ERROR "SOURCE_DIR and PATCH_FILE are required")
endif()

set(_contact_header
    "${SOURCE_DIR}/Jolt/Physics/Collision/ContactListener.h")
if(NOT EXISTS "${_contact_header}")
    message(FATAL_ERROR "Jolt source tree is incomplete: ${_contact_header}")
endif()

file(READ "${_contact_header}" _contact_header_contents)
string(FIND "${_contact_header_contents}"
    "mCombinedFriction2" _already_patched)
if(NOT _already_patched EQUAL -1)
    message(STATUS "Jolt source anisotropic-contact patch already applied")
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
