# SPDX-FileCopyrightText: 2026 The ZIMH Project
# SPDX-License-Identifier: MIT

## windows-api-target.cmake
##
## Validate the Windows API version selected by the compiler, SDK, and build
## flags. This module intentionally does not define WINVER or _WIN32_WINNT:
## current SDK defaults should expose the normal modern API surface. The check
## only rejects builds that target an older Windows API level.

include(CheckCSourceCompiles)

set(ZIMH_MIN_WINDOWS_API_VERSION "0x0A00" CACHE STRING
    "Minimum Windows API target accepted by ZIMH.")

if (WIN32)
    check_c_source_compiles("
        #include <winsdkver.h>
        #include <sdkddkver.h>

        #if !defined(WINVER)
        #error WINVER is not defined after including Windows SDK version headers.
        #endif

        #if !defined(_WIN32_WINNT)
        #error _WIN32_WINNT is not defined after including Windows SDK version headers.
        #endif

        #if WINVER < ${ZIMH_MIN_WINDOWS_API_VERSION}
        #error WINVER targets an unsupported Windows API level.
        #endif

        #if _WIN32_WINNT < ${ZIMH_MIN_WINDOWS_API_VERSION}
        #error _WIN32_WINNT targets an unsupported Windows API level.
        #endif

        int main(void) { return 0; }
    " ZIMH_WINDOWS_API_TARGET_OK)

    if (NOT ZIMH_WINDOWS_API_TARGET_OK)
        message(FATAL_ERROR
            "ZIMH requires a Windows 10 or newer API target "
            "(${ZIMH_MIN_WINDOWS_API_VERSION} or newer). Do not configure "
            "WINVER or _WIN32_WINNT below ${ZIMH_MIN_WINDOWS_API_VERSION}.")
    endif ()
endif ()
