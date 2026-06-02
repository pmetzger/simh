## =============================================================================
## FindPCAP.cmake (Modernized Adapter)
## =============================================================================
include(FindPackageHandleStandardArgs)

# 1. Try Config-mode first (vcpkg provides this natively on Windows)
find_package(PCAP QUIET CONFIG)

# 2. Try PkgConfig for Linux/macOS
if (NOT PCAP_FOUND)
    find_package(PkgConfig QUIET)
    if (PKG_CONFIG_FOUND)
        pkg_check_modules(PC_PCAP QUIET IMPORTED_TARGET libpcap)
        if (TARGET PkgConfig::PC_PCAP)
            add_library(PCAP::PCAP ALIAS PkgConfig::PC_PCAP)
            set(PCAP_FOUND TRUE)
        endif()
    endif()
endif()

# 3. Handle standard search fallback if both above fail
if (NOT PCAP_FOUND)
    find_path(PCAP_INCLUDE_DIR NAMES pcap.h)
    find_library(PCAP_LIBRARY NAMES pcap wpcap)

    if (PCAP_INCLUDE_DIR AND PCAP_LIBRARY)
        add_library(PCAP::PCAP UNKNOWN IMPORTED)
        set_target_properties(PCAP::PCAP PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${PCAP_INCLUDE_DIR}"
            IMPORTED_LOCATION "${PCAP_LIBRARY}"
        )
        set(PCAP_FOUND TRUE)
    endif()
endif()

find_package_handle_standard_args(PCAP DEFAULT_MSG PCAP_FOUND)
