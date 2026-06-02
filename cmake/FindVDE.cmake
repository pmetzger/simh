## =============================================================================
## FindVDE.cmake (Modernized Adapter)
## =============================================================================
include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
    pkg_check_modules(PC_VDE QUIET IMPORTED_TARGET vdeplug)
endif()

if (TARGET PkgConfig::PC_VDE)
    add_library(VDE::VDE ALIAS PkgConfig::PC_VDE)
    set(VDE_FOUND TRUE)
else()
    # Manual fallback for systems where pkg-config might be missing
    find_path(VDE_INCLUDE_DIR NAMES libvdeplug.h PATH_SUFFIXES vde2)
    find_library(VDE_LIBRARY NAMES vdeplug)

    if (VDE_INCLUDE_DIR AND VDE_LIBRARY)
        add_library(VDE::VDE UNKNOWN IMPORTED)
        set_target_properties(VDE::VDE PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${VDE_INCLUDE_DIR}"
            IMPORTED_LOCATION "${VDE_LIBRARY}"
        )
        set(VDE_FOUND TRUE)
    endif()
endif()

find_package_handle_standard_args(VDE DEFAULT_MSG VDE_FOUND)
