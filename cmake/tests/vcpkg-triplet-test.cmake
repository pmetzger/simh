# SPDX-FileCopyrightText: 2026 The ZIMH Project
# SPDX-License-Identifier: MIT

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/..")
include(vcpkg-triplet)

## Fail the script when an actual value does not match the expected value.
function(assert_equal actual expected label)
    if (NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR
            "${label}: expected '${expected}', got '${actual}'")
    endif ()
endfunction ()

## Clear all caller-scope variables that can affect triplet selection.
macro(clear_triplet_inputs)
    unset(VCPKG_TARGET_TRIPLET)
    unset(VCPKG_TARGET_TRIPLET CACHE)
    unset(CMAKE_GENERATOR_PLATFORM)
    unset(CMAKE_HOST_SYSTEM_PROCESSOR)
    unset(CMAKE_SYSTEM_NAME)
    unset(CMAKE_C_COMPILER_ID)
    unset(BUILD_SHARED_DEPS)
    unset(MSVC)
    unset(MINGW)
    set(ENV{VCPKG_DEFAULT_TRIPLET})
endmacro ()

clear_triplet_inputs()
set(ENV{VCPKG_DEFAULT_TRIPLET} "arm64-windows-static")
zimh_select_vcpkg_triplet(triplet)
assert_equal("${triplet}" "arm64-windows-static"
             "VCPKG_DEFAULT_TRIPLET environment value")

clear_triplet_inputs()
set(VCPKG_TARGET_TRIPLET "x86-windows")
set(ENV{VCPKG_DEFAULT_TRIPLET} "arm64-windows-static")
zimh_select_vcpkg_triplet(triplet)
assert_equal("${triplet}" "x86-windows"
             "VCPKG_TARGET_TRIPLET takes precedence")

clear_triplet_inputs()
set(CMAKE_SYSTEM_NAME "Windows")
set(CMAKE_C_COMPILER_ID "MSVC")
set(MSVC TRUE)
set(BUILD_SHARED_DEPS TRUE)
zimh_select_vcpkg_triplet(triplet)
assert_equal("${triplet}" "x64-windows" "MSVC shared Windows triplet")

clear_triplet_inputs()
set(CMAKE_SYSTEM_NAME "Windows")
set(CMAKE_C_COMPILER_ID "MSVC")
set(CMAKE_GENERATOR_PLATFORM "Win32")
set(MSVC TRUE)
set(BUILD_SHARED_DEPS FALSE)
zimh_select_vcpkg_triplet(triplet)
assert_equal("${triplet}" "x86-windows-static"
             "MSVC static Win32 triplet")

clear_triplet_inputs()
set(CMAKE_SYSTEM_NAME "Windows")
set(CMAKE_C_COMPILER_ID "GNU")
set(MINGW TRUE)
zimh_select_vcpkg_triplet(triplet)
assert_equal("${triplet}" "x64-mingw-dynamic" "MinGW triplet")

clear_triplet_inputs()
set(CMAKE_SYSTEM_NAME "Linux")
set(CMAKE_HOST_SYSTEM_PROCESSOR "aarch64")
zimh_select_vcpkg_triplet(triplet)
assert_equal("${triplet}" "arm64-linux" "Linux arm64 triplet")

clear_triplet_inputs()
set(CMAKE_SYSTEM_NAME "Darwin")
set(CMAKE_HOST_SYSTEM_PROCESSOR "arm64")
zimh_select_vcpkg_triplet(triplet)
assert_equal("${triplet}" "arm64-osx" "macOS arm64 triplet")

clear_triplet_inputs()
set(CMAKE_SYSTEM_NAME "Linux")
set(CMAKE_HOST_SYSTEM_PROCESSOR "x86_64")
zimh_configure_vcpkg_triplet()
assert_equal("${VCPKG_TARGET_TRIPLET}" "x64-linux"
             "configured cache triplet")

message(STATUS "vcpkg-triplet-test passed")
