# SPDX-FileCopyrightText: 2026 The ZIMH Project
# SPDX-License-Identifier: MIT

## vcpkg-triplet.cmake
##
## Select and configure the vcpkg target triplet used by ZIMH.

include_guard(GLOBAL)

## Select the vcpkg triplet that should be used for this configure run.
## Explicit user configuration wins first, followed by VCPKG_DEFAULT_TRIPLET
## from the environment. When neither is supplied, choose a native triplet
## from the host platform, compiler family, and BUILD_SHARED_DEPS policy.
function(zimh_select_vcpkg_triplet out_var)
    if (DEFINED VCPKG_TARGET_TRIPLET AND
        NOT "${VCPKG_TARGET_TRIPLET}" STREQUAL "")
        set(${out_var} "${VCPKG_TARGET_TRIPLET}" PARENT_SCOPE)
        return ()
    endif ()

    if (DEFINED ENV{VCPKG_DEFAULT_TRIPLET} AND
        NOT "$ENV{VCPKG_DEFAULT_TRIPLET}" STREQUAL "")
        set(${out_var} "$ENV{VCPKG_DEFAULT_TRIPLET}" PARENT_SCOPE)
        return ()
    endif ()

    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(_arch "x64")
        if (CMAKE_GENERATOR_PLATFORM MATCHES "[Ww][Ii][Nn]32")
            set(_arch "x86")
        elseif (CMAKE_GENERATOR_PLATFORM MATCHES "[Aa][Rr][Mm]64")
            set(_arch "arm64")
        elseif (CMAKE_GENERATOR_PLATFORM MATCHES "[Aa][Rr][Mm]")
            set(_arch "arm")
        endif ()

        if (MSVC OR CMAKE_C_COMPILER_ID MATCHES ".*Clang")
            set(_platform "windows")
            if (NOT BUILD_SHARED_DEPS)
                set(_runtime "static")
            endif ()
        elseif (MINGW OR CMAKE_C_COMPILER_ID STREQUAL "GNU")
            set(_platform "mingw")
            set(_runtime "dynamic")
        endif ()
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "aarch64")
            set(_arch "arm64")
        else ()
            set(_arch "x64")
        endif ()
        set(_platform "linux")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        ## This native host guess is intentionally narrow. Universal or
        ## cross-architecture macOS vcpkg builds should set
        ## VCPKG_TARGET_TRIPLET or VCPKG_DEFAULT_TRIPLET explicitly.
        if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
            set(_arch "arm64")
        else ()
            set(_arch "x64")
        endif ()
        set(_platform "osx")
    else ()
        message(STATUS "CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}")
        message(STATUS
            "CMAKE_HOST_SYSTEM_PROCESSOR=${CMAKE_HOST_SYSTEM_PROCESSOR}")
        message(FATAL_ERROR
            "Could not determine VCPKG platform and system triplet.\n"
            "(a) Are you sure that VCPKG is usable on this system? Check "
            "VCPKG_ROOT and ensure that you have properly bootstrapped "
            "VCPKG.\n"
            "(b) If VCPKG is not usable on this system, unset the VCPKG_ROOT "
            "environment variable.")
    endif ()

    if (NOT _arch OR NOT _platform)
        message(FATAL_ERROR
            "Could not determine VCPKG platform and system triplet.")
    endif ()

    set(_triplet "${_arch}-${_platform}")
    if (_runtime)
        string(APPEND _triplet "-${_runtime}")
    endif ()

    set(${out_var} "${_triplet}" PARENT_SCOPE)
endfunction ()

## Store the selected vcpkg triplet in the cache variable consumed by the
## vcpkg CMake toolchain file.
function(zimh_configure_vcpkg_triplet)
    zimh_select_vcpkg_triplet(_triplet)
    set(VCPKG_TARGET_TRIPLET "${_triplet}" CACHE STRING
        "Vcpkg target triplet (ex. x86-windows)" FORCE)
endfunction ()
