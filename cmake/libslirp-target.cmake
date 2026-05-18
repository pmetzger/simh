## libslirp-target.cmake
##
## Build the normalized internal SLIRP dependency target used by ZIMH.

if (WITH_NETWORK AND WITH_SLIRP AND TARGET PkgConfig::LIBSLIRP AND
    NOT TARGET ZIMH::LIBSLIRP)
    add_library(ZIMH::LIBSLIRP INTERFACE IMPORTED)
    target_link_libraries(ZIMH::LIBSLIRP INTERFACE PkgConfig::LIBSLIRP)
endif ()

if (WITH_NETWORK AND WITH_SLIRP AND WIN32 AND NOT MINGW AND
    NOT TARGET ZIMH::LIBSLIRP)
    ## vcpkg's libslirp package does not currently provide a CMake package.
    ## This fallback creates the same internal target from the installed
    ## headers and static-library dependency closure.
    find_path(ZIMH_LIBSLIRP_INCLUDE_DIR
        NAMES slirp/libslirp.h
        PATH_SUFFIXES include)
    find_path(ZIMH_GLIB_INCLUDE_DIR
        NAMES glib.h
        PATH_SUFFIXES include/glib-2.0)
    find_path(ZIMH_GLIB_CONFIG_INCLUDE_DIR
        NAMES glibconfig.h
        PATH_SUFFIXES lib/glib-2.0/include)
    find_library(ZIMH_LIBSLIRP_LIBRARY NAMES slirp)
    find_library(ZIMH_GLIB_LIBRARY NAMES glib-2.0)
    find_library(ZIMH_INTL_LIBRARY NAMES intl)
    find_library(ZIMH_ICONV_LIBRARY NAMES iconv)
    find_library(ZIMH_PCRE2_LIBRARY NAMES pcre2-8)

    set(ZIMH_LIBSLIRP_VERSION_OK FALSE)
    if (ZIMH_LIBSLIRP_INCLUDE_DIR)
        set(ZIMH_LIBSLIRP_VERSION_HEADER
            "${ZIMH_LIBSLIRP_INCLUDE_DIR}/slirp/libslirp-version.h")
        if (EXISTS "${ZIMH_LIBSLIRP_VERSION_HEADER}")
            file(STRINGS "${ZIMH_LIBSLIRP_VERSION_HEADER}"
                 ZIMH_LIBSLIRP_VERSION_LINE
                 REGEX "^#define[ \t]+SLIRP_VERSION_STRING[ \t]+\"[^\"]+\"")
            string(REGEX REPLACE
                   "^#define[ \t]+SLIRP_VERSION_STRING[ \t]+\"([^\"]+)\".*$"
                   "\\1"
                   ZIMH_LIBSLIRP_VERSION
                   "${ZIMH_LIBSLIRP_VERSION_LINE}")
            if (ZIMH_LIBSLIRP_VERSION AND
                ZIMH_LIBSLIRP_VERSION VERSION_GREATER_EQUAL
                LIBSLIRP_MIN_VERSION)
                set(ZIMH_LIBSLIRP_VERSION_OK TRUE)
            endif ()
        endif ()
    endif ()

    if (ZIMH_LIBSLIRP_VERSION_OK AND
        ZIMH_LIBSLIRP_INCLUDE_DIR AND
        ZIMH_GLIB_INCLUDE_DIR AND
        ZIMH_GLIB_CONFIG_INCLUDE_DIR AND
        ZIMH_LIBSLIRP_LIBRARY AND
        ZIMH_GLIB_LIBRARY AND
        ZIMH_INTL_LIBRARY AND
        ZIMH_ICONV_LIBRARY AND
        ZIMH_PCRE2_LIBRARY)
        add_library(ZIMH::LIBSLIRP INTERFACE IMPORTED)
        target_include_directories(ZIMH::LIBSLIRP INTERFACE
            "${ZIMH_LIBSLIRP_INCLUDE_DIR}"
            "${ZIMH_GLIB_INCLUDE_DIR}"
            "${ZIMH_GLIB_CONFIG_INCLUDE_DIR}")
        target_compile_definitions(ZIMH::LIBSLIRP INTERFACE
            LIBSLIRP_STATIC
            PCRE2_STATIC)
        target_link_libraries(ZIMH::LIBSLIRP INTERFACE
            "${ZIMH_LIBSLIRP_LIBRARY}"
            "${ZIMH_GLIB_LIBRARY}"
            "${ZIMH_INTL_LIBRARY}"
            "${ZIMH_ICONV_LIBRARY}"
            "${ZIMH_PCRE2_LIBRARY}"
            ws2_32
            iphlpapi
            winmm)
        set(LIBSLIRP_FOUND TRUE)
        set(LIBSLIRP_VERSION "${ZIMH_LIBSLIRP_VERSION}")
    endif ()

    unset(ZIMH_LIBSLIRP_VERSION_HEADER)
    unset(ZIMH_LIBSLIRP_VERSION_LINE)
    unset(ZIMH_LIBSLIRP_VERSION_OK)
endif ()
