# =============================================================================
# FindPCRE2.cmake (Multi-Config Aware)
# =============================================================================

include(SelectLibraryConfigurations)

# 1. pkg-config hints (Primarily for Linux/FreeBSD/macOS)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_PCRE2 QUIET libpcre2-8)
endif()

# 2. Locate the Header
find_path(PCRE2_INCLUDE_DIR
    NAMES pcre2.h
    HINTS ${PC_PCRE2_INCLUDE_DIRS}
    PATH_SUFFIXES pcre2
)

# 3. Locate BOTH Release and Debug Libraries (Crucial for Windows/MSVC)
find_library(PCRE2_LIBRARY_RELEASE
    NAMES pcre2-8 pcre2
    HINTS ${PC_PCRE2_LIBRARY_DIRS}
)

find_library(PCRE2_LIBRARY_DEBUG
    NAMES pcre2-8d pcre2d
    HINTS ${PC_PCRE2_LIBRARY_DIRS}
)

# This standard CMake macro automatically evaluates the _RELEASE and _DEBUG 
# variables and populates PCRE2_LIBRARY appropriately.
select_library_configurations(PCRE2)

# 4. Standardize output
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PCRE2
    FOUND_VAR PCRE2_FOUND
    REQUIRED_VARS PCRE2_LIBRARY PCRE2_INCLUDE_DIR
    VERSION_VAR PC_PCRE2_VERSION
)

# 5. Build the Multi-Config Target
if(PCRE2_FOUND AND NOT TARGET PCRE2::PCRE2)
    add_library(PCRE2::PCRE2 UNKNOWN IMPORTED)
    
    set_target_properties(PCRE2::PCRE2 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${PCRE2_INCLUDE_DIR}"
    )

    # Bind Release Binary
    if(PCRE2_LIBRARY_RELEASE)
        set_property(TARGET PCRE2::PCRE2 APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(PCRE2::PCRE2 PROPERTIES IMPORTED_LOCATION_RELEASE "${PCRE2_LIBRARY_RELEASE}")
    endif()

    # Bind Debug Binary
    if(PCRE2_LIBRARY_DEBUG)
        set_property(TARGET PCRE2::PCRE2 APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(PCRE2::PCRE2 PROPERTIES IMPORTED_LOCATION_DEBUG "${PCRE2_LIBRARY_DEBUG}")
    endif()

    # Fallback for single-config platforms (e.g., standard Linux package managers)
    if(NOT PCRE2_LIBRARY_RELEASE AND NOT PCRE2_LIBRARY_DEBUG AND PCRE2_LIBRARY)
        set_target_properties(PCRE2::PCRE2 PROPERTIES IMPORTED_LOCATION "${PCRE2_LIBRARY}")
    endif()

    if(PC_PCRE2_CFLAGS_OTHER)
        set_target_properties(PCRE2::PCRE2 PROPERTIES INTERFACE_COMPILE_OPTIONS "${PC_PCRE2_CFLAGS_OTHER}")
    endif()
endif()

mark_as_advanced(PCRE2_INCLUDE_DIR PCRE2_LIBRARY PCRE2_LIBRARY_RELEASE PCRE2_LIBRARY_DEBUG)
