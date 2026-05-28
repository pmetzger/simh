include("${CMAKE_CURRENT_LIST_DIR}/../windows-api-target.cmake")

if (DEFINED ZIMH_WINDOWS_API_TARGET_OK)
    message(FATAL_ERROR
        "windows-api-target should not run compiler probes on non-Windows hosts")
endif ()

message(STATUS "windows-api-target-test passed")
