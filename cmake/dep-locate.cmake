##=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
## dep-locate.cmake
##
## Locate host-provided runtime dependencies for ZIMH. Missing required
## dependencies are reported through dependency-report.cmake so users get a
## single actionable configure failure.
##=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=

find_package(PCRE2)

if (WIN32)
    set(LIBSLIRP_MIN_VERSION 4.9.0)
else ()
    set(LIBSLIRP_MIN_VERSION 4.7.0)
endif ()
set(LIBSLIRP_PROBE
    "pkg-config slirp >= ${LIBSLIRP_MIN_VERSION} or vcpkg libslirp")

set(ZLIB_USE_STATIC_LIBS ON)
find_package(ZLIB)

if (WITH_VIDEO)
    if (NOT USING_VCPKG)
        find_package(PNG)
        find_package(Freetype)
        find_package(SDL2 NAMES sdl2 SDL2)
        find_package(SDL2_ttf NAMES sdl2_ttf SDL2_ttf)
    else ()
        find_package(PNG REQUIRED)
        find_package(SDL2 CONFIG)
        find_package(SDL2_ttf CONFIG)
    endif ()
endif ()

if (WITH_NETWORK)
    if (WITH_VDE)
        find_package(VDE QUIET)
    endif ()

    if (WITH_PCAP)
        find_package(PCAP)
    endif ()
endif ()

if (NOT WIN32 OR MINGW)
    find_package(PkgConfig QUIET)
    if (PKG_CONFIG_FOUND)
        if (NOT PCRE2_FOUND)
            pkg_check_modules(PCRE2 IMPORTED_TARGET libpcre2-8)
        endif ()

        if (NOT ZLIB_FOUND)
            pkg_check_modules(ZLIB IMPORTED_TARGET zlib)
        endif ()

        if (WITH_VIDEO)
            if (NOT PNG_FOUND)
                pkg_check_modules(PNG IMPORTED_TARGET libpng16)
            endif ()
            if (NOT SDL2_FOUND)
                pkg_check_modules(SDL2 IMPORTED_TARGET sdl2)
                if (NOT SDL2_FOUND)
                    pkg_check_modules(SDL2 IMPORTED_TARGET SDL2)
                endif ()
            endif ()

            if (NOT SDL2_ttf_FOUND)
                pkg_check_modules(SDL2_ttf IMPORTED_TARGET SDL2_ttf)
                if (NOT SDL2_ttf_FOUND)
                    pkg_check_modules(SDL2_ttf IMPORTED_TARGET sdl2_ttf)
                endif ()
            endif ()

            if (NOT FREETYPE_FOUND)
                pkg_check_modules(FREETYPE IMPORTED_TARGET freetype2)
            endif ()
        endif ()

        if (WITH_NETWORK)
            if (WITH_VDE AND NOT VDE_FOUND)
                pkg_check_modules(VDE QUIET IMPORTED_TARGET vdeplug)
            endif ()
            if (WITH_SLIRP AND (NOT LIBSLIRP_FOUND OR
                                NOT TARGET PkgConfig::LIBSLIRP))
                pkg_check_modules(LIBSLIRP IMPORTED_TARGET
                                  "slirp>=${LIBSLIRP_MIN_VERSION}")
            endif ()
        endif ()
    endif ()
endif ()

include(libslirp-target)

if (NOT ZLIB_FOUND)
    zimh_record_missing_dependency(
        NAME "zlib"
        REASON "compressed file support requires it"
        PROBE "CMake package ZLIB or pkg-config package zlib"
        PACKAGE_KEY ZLIB)
endif ()

if (NOT PCRE2_FOUND)
    zimh_record_missing_dependency(
        NAME "libpcre2"
        REASON "regular expression support requires it"
        PROBE "CMake package PCRE2 or pkg-config package libpcre2-8"
        PACKAGE_KEY PCRE2)
endif ()

if (WITH_VIDEO)
    if (NOT PNG_FOUND)
        zimh_record_missing_dependency(
            NAME "libpng"
            REASON "WITH_VIDEO=ON"
            PROBE "CMake package PNG or pkg-config package libpng16"
            DISABLE "-DWITH_VIDEO=OFF"
            PACKAGE_KEY PNG)
    endif ()
    if (NOT SDL2_FOUND)
        zimh_record_missing_dependency(
            NAME "SDL2"
            REASON "WITH_VIDEO=ON"
            PROBE "CMake package SDL2 or pkg-config package sdl2"
            DISABLE "-DWITH_VIDEO=OFF"
            PACKAGE_KEY SDL2)
    endif ()
    if (NOT FREETYPE_FOUND)
        zimh_record_missing_dependency(
            NAME "freetype"
            REASON "WITH_VIDEO=ON"
            PROBE "CMake package Freetype or pkg-config package freetype2"
            DISABLE "-DWITH_VIDEO=OFF"
            PACKAGE_KEY FREETYPE)
    endif ()
    if (NOT SDL2_ttf_FOUND)
        zimh_record_missing_dependency(
            NAME "SDL2_ttf"
            REASON "WITH_VIDEO=ON"
            PROBE "CMake package SDL2_ttf or pkg-config package SDL2_ttf"
            DISABLE "-DWITH_VIDEO=OFF"
            PACKAGE_KEY SDL2_TTF)
    endif ()
endif ()

if (WITH_NETWORK AND WITH_PCAP AND NOT PCAP_FOUND)
    zimh_record_missing_dependency(
        NAME "libpcap"
        REASON "WITH_NETWORK=ON and WITH_PCAP=ON"
        PROBE "pcap headers through CMake package PCAP"
        DISABLE "-DWITH_PCAP=OFF"
        PACKAGE_KEY PCAP)
endif ()

if (WITH_NETWORK AND WITH_SLIRP AND NOT TARGET ZIMH::LIBSLIRP)
    zimh_record_missing_dependency(
        NAME "libslirp"
        VERSION ">= ${LIBSLIRP_MIN_VERSION}"
        REASON "WITH_NETWORK=ON and WITH_SLIRP=ON"
        PROBE "${LIBSLIRP_PROBE}"
        DISABLE "-DWITH_SLIRP=OFF"
        PACKAGE_KEY LIBSLIRP)
endif ()
