## Note: C_STANDARD 17 and C_STANDARD 23 require CMake 3.21 or better.
cmake_minimum_required(VERSION 3.21)

## Let developers intentionally configure a build tree with a non-default
## compiler, e.g. GNU GCC on a Clang-oriented host, to shake out compiler
## evaluation-order and warning differences. This must run before project().
set(ZIMH_C_COMPILER "" CACHE FILEPATH
    "Optional non-default C compiler for this build tree.")
if (ZIMH_C_COMPILER)
    if (DEFINED CMAKE_C_COMPILER AND
        NOT CMAKE_C_COMPILER STREQUAL ZIMH_C_COMPILER)
        message(FATAL_ERROR
            "ZIMH_C_COMPILER must be set before the build tree chooses "
            "CMAKE_C_COMPILER. Use a fresh build directory.")
    endif ()

    set(CMAKE_C_COMPILER "${ZIMH_C_COMPILER}" CACHE FILEPATH
        "C compiler" FORCE)
endif ()

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR
         ""
        "!!! DO NOT BUILD CMake artifacts in the SIMH source directory !!!\n"
        ""
        "Create a subdirectory and build in that subdirectory, e.g.:"
        "\n"
        "  $ mkdir -p build/release\n"
        "  $ cd build/release\n"
        "  $ cmake -G \"your generator here\" ../..\n"
        ""
        "Preventing in-tree source build.")
endif ()

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
set(ZIMH_VERSION_PATCH 25)
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
file(MAKE_DIRECTORY "${ZIMH_GENERATED_INCLUDE_DIR}")
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/zimh_version.h.in"
    "${ZIMH_GENERATED_INCLUDE_DIR}/zimh_version.h"
    @ONLY)

# Places to look for CMake modules/includes
set(SIMH_INCLUDE_PATH_LIST
    ${CMAKE_SOURCE_DIR}/cmake
    ${CMAKE_SOURCE_DIR}/cmake/installer-customizations)
list(APPEND CMAKE_MODULE_PATH ${SIMH_INCLUDE_PATH_LIST})
message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

## vcpkg sanity checking: Cannot use vcpkg and XP toolkit together. If this is
## a XP build, disable vcpkg:
set(USING_VCPKG FALSE)
if (DEFINED ENV{VCPKG_ROOT})
    if (CMAKE_GENERATOR_TOOLSET MATCHES "v[0-9][0-9][0-9]_xp")
        message(FATAL_ERROR
            "Configuration conflict: Cannot build XP binaries with vcpkg. Either "
            "unset VCPKG_ROOT in your environment or remove the '-T' toolkit option."
            "\n"
            "Also remove CMakeCache.txt and recursively remove the CMakeFiles "
            "subdirectory in your build folder before reconfiguring.")
    endif ()

    set(USING_VCPKG TRUE)

    ## Defer loading the CMAKE_TOOLCHAIN_FILE:
    set(SIMH_CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    if(DEFINED CMAKE_TOOLCHAIN_FILE)
        ## Use the user's provided toolchain file, but load it later.
        message(STATUS "Deferring CMAKE_TOOLCHAIN_FILE load.")
        set(SIMH_CMAKE_TOOLCHAIN_FILE "${CMAKE_TOOLCHAIN_FILE}")
        unset(CMAKE_TOOLCHAIN_FILE)
        unset(CMAKE_TOOLCHAIN_FILE CACHE)
    endif()

    file(TO_CMAKE_PATH "${SIMH_CMAKE_TOOLCHAIN_FILE}" SIMH_CMAKE_TOOLCHAIN_FILE)
    message(STATUS "SIMH_CMAKE_TOOLCHAIN_FILE is ${SIMH_CMAKE_TOOLCHAIN_FILE}")
endif ()

## Respect MSVC_RUNTIME_LIBRARY's setting. the policy has to be set before
## project(), otherwise it'd have been put into platform-quirks.
##
## Note: See cmake\build_dep_matrix.cmake to see how this is propagated down
## into the dependency libraries.
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.15)
    cmake_policy(SET CMP0091 NEW)
endif()

project(zimh VERSION "${ZIMH_VERSION}" LANGUAGES C)

# zimh actively supports C11 and above. C99 is not supported,
# but we try not to break C99 unnecessarily.
if (DEFINED CMAKE_C_STANDARD)
    set(ZIMH_DEFAULT_C_STANDARD "${CMAKE_C_STANDARD}")
else ()
    set(ZIMH_DEFAULT_C_STANDARD "17")
endif ()
set(ZIMH_C_STANDARD "${ZIMH_DEFAULT_C_STANDARD}" CACHE STRING
    "C language standard for zimh-owned targets.")
unset(ZIMH_DEFAULT_C_STANDARD)

set(CMAKE_C_STANDARD "${ZIMH_C_STANDARD}")
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(vcpkg-setup)
include(GNUInstallDirs)

## Provide a default CMAKE_BUILD_TYPE if CMAKE_CONFIGURATION_TYPES is empty or not defined.
if (NOT CMAKE_CONFIGURATION_TYPES)
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release")
        message(STATUS "CMAKE_BUILD_TYPE defaulted to ${CMAKE_BUILD_TYPE}")
    else (NOT CMAKE_BUILD_TYPE)
        message(STATUS "CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")
    endif (NOT CMAKE_BUILD_TYPE)
endif ()

# SIMH_SYSTEM_ID: Roughly analogous to the autoconf system triple. Used (almost exclusively)
# as part of the dependencies' top-level directory name.
set(SIMH_SYSTEM_ID ${CMAKE_SYSTEM_NAME})
string(REPLACE "." ";" version_list ${CMAKE_SYSTEM_VERSION})
list(GET version_list 0 version_major)
string(APPEND SIMH_SYSTEM_ID "-" ${version_major})

if (CMAKE_C_LIBRARY_ARCHITECTURE)
    string(APPEND SIMH_SYSTEM_ID -${CMAKE_C_LIBRARY_ARCHITECTURE})
endif (CMAKE_C_LIBRARY_ARCHITECTURE)
string(APPEND SIMH_SYSTEM_ID -${CMAKE_C_COMPILER_ID})
string(REPLACE "." ";" version_list ${CMAKE_C_COMPILER_VERSION})
list(GET version_list 0 version_major)
list(GET version_list 1 version_minor)
if (NOT version_minor)
  set(version_minor 0)
endif ()
string(APPEND SIMH_SYSTEM_ID "-${version_major}.${version_minor}")
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    string(APPEND SIMH_SYSTEM_ID "-64")
else ()
    string(APPEND SIMH_SYSTEM_ID "-32")
endif ()
if (CMAKE_GENERATOR MATCHES "Visual Studio .*" AND CMAKE_GENERATOR_TOOLSET MATCHES "v[0-9][0-9][0-9]_xp")
    string(APPEND SIMH_SYSTEM_ID "-XP")
endif ()

set(SIMH_CMAKE_RUNTIME_DEPENDENCIES_SUPPORTED FALSE)
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.21" AND
    (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR
     CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
     WIN32))
    ## CMake documents file(GET_RUNTIME_DEPENDENCIES), and therefore
    ## install(RUNTIME_DEPENDENCY_SET), as supporting Windows, Linux, and
    ## macOS.  Revisit this list if CMake adds support for more platforms.
    set(SIMH_CMAKE_RUNTIME_DEPENDENCIES_SUPPORTED TRUE)
endif ()

##-- Options --##
set(MAC_UNIVERSAL_OPTVAL FALSE)
if (NOT DEFINED MAC_UNIVERSAL)
    if (APPLE)
        message("macOS universal binaries WILL NOT BE BUILT")
    endif ()
else ()
    set(MAC_UNIVERSAL_OPTVAL ${MAC_UNIVERSAL})
    if (MAC_UNIVERSAL_OPTVAL)
        message(STATUS "macOS universal binaries WILL BE BUILT.")
    else ()
        message(STATUS "macOS universal binaries NOT WILL BE BUILT.")
    endif ()
endif ()

option(BUILD_SHARED_DEPS
       "Prefer shared dependency runtime linkage when supported by the dependency provider."
       FALSE)
option(WITH_ASYNC
       "Enable (=1)/disable (=0) Asynchronous I/O and threading."
       TRUE)
option(WITH_NETWORK
       "Enable (=1)/disable (=0) simulator networking support. (def: enabled)"
       TRUE)
option(WITH_PCAP
       "Enable (=1)/disable (=0) libpcap (packet capture) support. (def: enabled)"
       TRUE)
option(WITH_SLIRP
       "Enable (=1)/disable (=0) SLIRP network support. (def: enabled)"
       TRUE)
if (WIN32)
    set(WITH_VDE FALSE CACHE INTERNAL
        "VDE2/VDE4 network support is not available on Windows.")
else ()
    option(WITH_VDE
           "Enable (=1)/disable (=0) VDE2/VDE4 network support. (def: enabled)"
           TRUE)
endif ()
option(WITH_TAP
       "Enable (=1)/disable (=0) TAP/TUN device network support. (def: enabled)"
       TRUE)
option(WITH_VIDEO
       "Enable (=1)/disable (=0) simulator display and graphics support (def: enabled)"
       TRUE)
option(WITH_UNIT_TESTS
       "Enable (=1)/disable (=0) host-side unit tests using cmocka. (def: enabled)"
       TRUE)
option(WITH_ROMS
       "Enable (=1)/disable (=0) internal support ROM generation and embedding."
       TRUE)
option(ENABLE_CPPCHECK
       "Enable (=1)/disable (=0) 'cppcheck' static code analysis. (def: disabled.)"
       FALSE)
option(ENABLE_WINAPI_DEPRECATION_WARNINGS
       "Enable (=1)/disable (=0) WinAPI deprecation warnings (def: disabled)"
       FALSE)
option(WARNINGS_FATAL
       "Compile-time warnings are errors (e.g., \"-Werror\" on GCC)"
       FALSE)
option(RELEASE_LTO
       "Use Link Time Optimization (LTO) in Release builds."
       FALSE)
option(DEBUG_WALL
       "Turn on maximal compiler warnings in Debug builds (e.g., \"-Wall\" or \"/W4\")"
       FALSE)
option(DEBUG_WARNINGS
       "Turn on additional strict compiler warnings in Debug builds."
       FALSE)
option(ZIMH_PACKAGE_SUFFIX
       "Packaging suffix for CPack artifacts."
       "")
option(MAC_UNIVERSAL
       "macOS universal binary flag: TRUE -> build universal binaries, FALSE -> don't."
       ${MAC_UNIVERSAL_OPTVAL})

unset(NO_DEP_BUILD CACHE)

## Keep runtime outputs in the active build tree.
set(SIMH_RUNTIME_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${SIMH_RUNTIME_OUTPUT_DIR}")
foreach(_cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${_cfg}" _cfg_uc)
    set("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg_uc}"
        "${SIMH_RUNTIME_OUTPUT_DIR}")
endforeach ()

## Source directory is always on the global include path.
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${SIMH_INCLUDE_ROOT})

## "Standard" includes
include(CheckCSourceCompiles)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CheckCCompilerFlag)
include(FindPackageHandleStandardArgs)
include(dependency-report)

## Deal with platform quirkiness
include(platform-quirks)

set(THREADING_PKG_STATUS "unknown")
set(ZLIB_PKG_STATUS "unknown")
set(PCRE_PKG_STATUS "unknown")
set(VIDEO_PKG_STATUS)
set(NETWORK_PKG_STATUS)

# if (USING_VCPKG)
#     ## Sanity checking output: Ensure that vcpkg picked up the correct triplet
#     message(STATUS "VCPKG sanity check:\n"
#         "    .. VCPKG target triplet is ${VCPKG_TARGET_TRIPLET}\n"
#         "    .. VCPKG_CRT_LINKAGE is ${VCPKG_CRT_LINKAGE}"
#         ## "    .. CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}\n"
#         ## "    .. CMAKE_MODULE_PATH=${CMAKE_MODULE_PATH}
#     )
# endif ()

include(os-features)
include(dep-locate)
include(dep-link)

if (WITH_UNIT_TESTS)
    if (WIN32)
        find_package(cmocka CONFIG QUIET)
        if (TARGET cmocka::cmocka)
            set(CMOCKA_FOUND TRUE)
        endif ()
    else ()
        find_package(PkgConfig QUIET)
        if (PKG_CONFIG_FOUND)
            pkg_check_modules(CMOCKA IMPORTED_TARGET cmocka>=1.1.5)
        endif ()
    endif ()

    if (NOT CMOCKA_FOUND)
        zimh_record_missing_dependency(
            NAME "cmocka"
            VERSION ">= 1.1.5"
            REASON "WITH_UNIT_TESTS=ON"
            PROBE "cmocka CMake package or pkg-config package >= 1.1.5"
            DISABLE "-DWITH_UNIT_TESTS=OFF"
            PACKAGE_KEY CMOCKA)
    endif ()
endif ()

zimh_report_missing_dependencies()

if (VIDEO_PKG_STATUS)
    string(REPLACE ";" ", " VIDEO_PKG_STATUS "${VIDEO_PKG_STATUS}")
else (VIDEO_PKG_STATUS)
    set(VIDEO_PKG_STATUS "unknown")
endif (VIDEO_PKG_STATUS)

if (NETWORK_PKG_STATUS)
    string(REPLACE ";" ", " NETWORK_PKG_STATUS "${NETWORK_PKG_STATUS}")
else (NETWORK_PKG_STATUS)
    set(NETWORK_PKG_STATUS "unknown")
endif (NETWORK_PKG_STATUS)

set(CPPCHECK_STATUS "disabled.")

if (ENABLE_CPPCHECK)
    find_program(cppcheck_cmd NAMES cppcheck)
    if (cppcheck_cmd)
        list(APPEND cppcheck_cmd
             "--language=c"
             "--enable=warning,style,performance,portability,information,missingInclude"
             "--suppress=missingIncludeSystem"
             "--inline-suppr"
             "--std=c${ZIMH_C_STANDARD}"
             "--force")
        set(CPPCHECK_STATUS "found and enabled.")
        if (WIN32)
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                list(APPEND cppcheck_cmd
                     "--platform=win64")
                set(CPPCHECK_STATUS "found, Win64 platform.")
            else ()
                list(APPEND cppcheck_cmd
                     "--platform=win32A")
                set(CPPCHECK_STATUS "found, Win32 ASCII platform.")
            endif ()
        endif ()
    else (cppcheck_cmd)
        set(CPPCHECK_STATUS "'cppcheck' not installed.")
    endif ()
endif ()

set(_feature_text "Libraries and features:\n")
string(APPEND _feature_text "\n    * Build with video/graphics support. ${BUILD_WITH_VIDEO}")
string(APPEND _feature_text "\n    * Build with networking support .... ${BUILD_WITH_NETWORK}")
string(APPEND _feature_text "\n    * Build internal ROMS .............. ")
if (WITH_ROMS)
    string(APPEND _feature_text "Yes")
else ()
    string(APPEND _feature_text "No")
endif ()
string(APPEND _feature_text "\n    * Thread support ................... ${THREADING_PKG_STATUS}")
string(APPEND _feature_text "\n    * zlib ............................. ${ZLIB_PKG_STATUS}")
string(APPEND _feature_text "\n    * Perl-Compatible RegExps........... ${PCRE_PKG_STATUS}")
string(APPEND _feature_text "\n    * PNG, Freetype, SDL2, SDL2_ttf .... ${VIDEO_PKG_STATUS}")
string(APPEND _feature_text "\n    * Network support .................. ${NETWORK_PKG_STATUS}")
if (WITH_UNIT_TESTS)
    string(APPEND _feature_text "\n    * Host-side unit tests ............. requested")
else ()
    string(APPEND _feature_text "\n    * Host-side unit tests ............. disabled")
endif ()

get_target_property(OS_FEATURE_DEFS os_features INTERFACE_COMPILE_DEFINITIONS)
list(LENGTH OS_FEATURE_DEFS len_os_features)
string(APPEND _feature_text "\n    * OS Feature definitions")
if (OS_FEATURE_DEFS)
    string(APPEND _feature_text ":")
    foreach (feature ${OS_FEATURE_DEFS})
        string(APPEND _feature_text "\n      .. ${feature}")
    endforeach()
else ()
    string(APPEND _feature_text " ........... None defined.")
endif ()

get_target_property(OS_FEATURE_LIBS os_features INTERFACE_LINK_LIBRARIES)
list(LENGTH OS_FEATURE_LIBS len_os_features)
string(APPEND _feature_text "\n    * OS Feature libraries")
if (OS_FEATURE_LIBS)
    string(APPEND _feature_text ":")
    foreach (feature ${OS_FEATURE_LIBS})
        string(APPEND _feature_text "\n      .. ${feature}")
    endforeach ()
else ()
    string(APPEND _feature_text " ............. None required.")
endif ()

string(APPEND _feature_text "\n    * 'cppcheck' ....................... ${CPPCHECK_STATUS}")
string(APPEND _feature_text "\n")

message(STATUS ${_feature_text})
unset(_feature_text)

unset(ROM_STATUS)

include (add_simulator)

message(STATUS "Adding simulators")
include(simh-simulators)
include(simh-extra-tools)

if (WITH_UNIT_TESTS)
    set(unittest_cmocka_target "")

    if (WIN32)
        if (TARGET cmocka::cmocka)
            set(unittest_cmocka_target cmocka::cmocka)
        endif ()
    else ()
        if (CMOCKA_FOUND)
            set(unittest_cmocka_target PkgConfig::CMOCKA)
        endif ()
    endif ()

    if (unittest_cmocka_target)
        add_library(unittest INTERFACE)
        add_subdirectory(tests/unit)
        target_link_libraries(unittest INTERFACE
            ${unittest_cmocka_target}
            os_features
        )
        message(STATUS "Host-side cmocka unit tests enabled")
    else ()
        message(STATUS "Host-side unit tests requested but cmocka was not found")
    endif ()

    unset(unittest_cmocka_target)
endif ()

get_property(SIMH_UNIT_TEST_TARGETS GLOBAL PROPERTY SIMH_UNIT_TEST_TARGETS)
if (SIMH_UNIT_TEST_TARGETS)
    list(REMOVE_DUPLICATES SIMH_UNIT_TEST_TARGETS)
    add_custom_target(unit-tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
                -L unit
        DEPENDS ${SIMH_UNIT_TEST_TARGETS}
        USES_TERMINAL
        COMMENT "Building and running host-side unit tests"
    )
endif ()

get_property(SIMH_INTEGRATION_TARGETS GLOBAL PROPERTY
             SIMH_INTEGRATION_TARGETS)
if (SIMH_INTEGRATION_TARGETS)
    list(REMOVE_DUPLICATES SIMH_INTEGRATION_TARGETS)
    add_custom_target(integration-tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
                -L integration
        DEPENDS ${SIMH_INTEGRATION_TARGETS}
        USES_TERMINAL
        COMMENT "Building and running simulator integration tests"
    )
endif ()

include(cpack-setup)
include(simh-packaging)
