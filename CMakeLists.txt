cmake_minimum_required(VERSION 3.21)

# Enforce strict out-of-source builds early
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR 
        "!!! DO NOT BUILD CMake artifacts in the source directory !!!\n"
        "Create a separate build directory, e.g., 'build/', and run cmake from there."
    )
endif()

# Safely auto-detect and inject vcpkg if the environment variable is present,
# but ONLY if the user hasn't specified their own toolchain file.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" 
        CACHE FILEPATH "Auto-detected vcpkg toolchain"
    )
    message(STATUS "Auto-detected VCPKG_ROOT: Using vcpkg toolchain.")
endif()

# MSVC Runtime Library abstraction policy (Must be set before project())
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.15)
    cmake_policy(SET CMP0091 NEW)
endif()

# Ensure compiler data exports:
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Well-known places:
set(SIMH_CORE_ROOT ${CMAKE_SOURCE_DIR}/src/core)
set(SIMH_COMPAT_ROOT ${CMAKE_SOURCE_DIR}/src/compat)
set(SIMH_INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src/include)
set(SIMH_LIB_ROOT ${CMAKE_SOURCE_DIR}/src/lib)
set(SIMH_RUNTIME_ROOT ${CMAKE_SOURCE_DIR}/src/runtime)
set(SIMH_COMPONENTS_ROOT ${CMAKE_SOURCE_DIR}/src/components)
set(SIMH_SIMULATOR_ROOT ${CMAKE_SOURCE_DIR}/simulators)
set(SIMH_THIRD_PARTY_ROOT ${CMAKE_SOURCE_DIR}/third_party)
set(SIMH_TOOLS_ROOT ${CMAKE_SOURCE_DIR}/tools)
set(SIMH_DOCS_ROOT ${CMAKE_SOURCE_DIR}/docs)
set(ZIMH_GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated")

## ZIMH Version variables:
set(ZIMH_VERSION_MAJOR 2026)
set(ZIMH_VERSION_MINOR 5)
set(ZIMH_VERSION_PATCH 29)
set(ZIMH_VERSION_TWEAK 0)
math(EXPR ZIMH_VERSION_YEAR_VALUE "${ZIMH_VERSION_MAJOR} * 10000")
math(EXPR ZIMH_VERSION_MONTH_VALUE "${ZIMH_VERSION_MINOR} * 100")
math(EXPR ZIMH_VERSION_DATE_BASE
     "${ZIMH_VERSION_YEAR_VALUE} + ${ZIMH_VERSION_MONTH_VALUE}")
math(EXPR ZIMH_VERSION_DATE
     "${ZIMH_VERSION_DATE_BASE} + ${ZIMH_VERSION_PATCH}")
if (ZIMH_VERSION_TWEAK EQUAL 0)
    set(ZIMH_VERSION
        "${ZIMH_VERSION_MAJOR}.${ZIMH_VERSION_MINOR}.${ZIMH_VERSION_PATCH}")
else ()
    string(CONCAT ZIMH_VERSION
           "${ZIMH_VERSION_MAJOR}.${ZIMH_VERSION_MINOR}."
           "${ZIMH_VERSION_PATCH}.${ZIMH_VERSION_TWEAK}")
endif ()

# Places to look for CMake modules/includes
list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_SOURCE_DIR}/cmake
    ${CMAKE_SOURCE_DIR}/cmake/installer-customizations)

# Launch the project!
project(zimh VERSION "${ZIMH_VERSION}" LANGUAGES C)

# Output the version file:
file(MAKE_DIRECTORY "${ZIMH_GENERATED_INCLUDE_DIR}")
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/zimh_version.h.in"
    "${ZIMH_GENERATED_INCLUDE_DIR}/zimh_version.h"
    @ONLY)

include(GNUInstallDirs)

# Default build type if multi-config generator isn't used
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

# Core Feature Options
option(BUILD_SHARED_DEPS "Prefer shared dependency runtime linkage." OFF)
option(WITH_ASYNC       "Enable Asynchronous I/O and threading." ON)
option(WITH_NETWORK     "Enable simulator networking support." ON)
option(WITH_PCAP        "Enable libpcap support." ON)
option(WITH_SLIRP       "Enable SLIRP network support." ON)
option(WITH_TAP         "Enable TAP/TUN device network support." ON)
option(WITH_VIDEO       "Enable simulator display and graphics support." ON)
option(WITH_REGEXP      "Enable regular expression support, expect command." ON)
option(WITH_UNIT_TESTS  "Enable host-side unit tests using cmocka." ON)
option(WITH_ROMS        "Enable internal support ROM generation." ON)

if(WIN32)
    set(WITH_VDE OFF CACHE INTERNAL "VDE network support is not available on Windows.")
else()
    option(WITH_VDE     "Enable VDE2/VDE4 network support." ON)
endif()

# Developer & QA Controls
option(ENABLE_CPPCHECK  "Enable 'cppcheck' static code analysis." OFF)
option(WARNINGS_FATAL   "Treat compile-time warnings as errors." OFF)
option(RELEASE_LTO      "Use Link Time Optimization (LTO) in Release builds." OFF)
option(DEBUG_WALL       "Turn on maximal compiler warnings in Debug builds." OFF)

# C dialect override
include(c-dialect)

# Modern Runtime Output Alignment
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
if (NOT DEFINED CMAKE_BUILD_TYPE)
    string(APPEND CMAKE_RUNTIME_OUTPUT_DIRECTORY "/$<CONFIG>")
endif ()
message(STATUS "Binary and runtime artifacts in ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
foreach(cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endforeach()

# Platform Quirks and Verification Hooks
include(platform-quirks)
include(os-features)
include(simh-dependencies)

# External Static Analysis Setup (cppcheck)
if(ENABLE_CPPCHECK)
    find_program(CPPCHECK_EXE NAMES cppcheck)
    if(CPPCHECK_EXE)
        set(CMAKE_C_CPPCHECK "${CPPCHECK_EXE};--language=c;--inline-suppr;--std=c${CMAKE_C_STANDARD};--force")
        if(WIN32)
            list(APPEND CMAKE_C_CPPCHECK "--platform=win${CMAKE_SIZEOF_VOID_P}")
        endif()
    endif()
endif()

# Target Generation Architecture
include(add_simulator)

message(STATUS "Adding simulators...")
include(simh-simulators)
include(simh-extra-tools)

# Unit Testing Setup (Target-driven CTest)
if(WITH_UNIT_TESTS)
    enable_testing()
    
    # Simple boilerplate script testing hooks
    function(add_cmake_script_unit_test test_name script_name)
        add_test(NAME ${test_name} 
                 COMMAND ${CMAKE_COMMAND} -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/cmake-tests/${test_name} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/${script_name}")
        set_tests_properties(${test_name} PROPERTIES LABELS "zimh;unit")
    endfunction()

    add_cmake_script_unit_test(zimh-cmake-dependency-report dependency-report-test.cmake)
    add_cmake_script_unit_test(zimh-cmake-find-pcap find-pcap-test.cmake)

    # Cmocka framework verification
    add_library(unittest INTERFACE)
    set(unittest_cmocka_target unittest)

    if(WIN32)
        find_package(cmocka CONFIG QUIET)
    else()
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(CMOCKA IMPORTED_TARGET cmocka>=1.1.5)
        endif()
    endif()

    if(TARGET cmocka::cmocka OR TARGET PkgConfig::CMOCKA)
        add_subdirectory(tests/unit)
        
        if (TARGET cmocka::cmocka)
            target_link_libraries(unittest INTERFACE cmocka::cmocka)
        endif()
        if(TARGET PkgConfig::CMOCKA)
            target_link_libraries(unittest INTERFACE PkgConfig::CMOCKA)
        endif()

        target_link_libraries(unittest INTERFACE os_features)
    endif()
endif()

# Packaging Hooks
include(cpack-setup)
include(simh-packaging)
