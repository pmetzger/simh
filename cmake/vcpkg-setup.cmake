##+=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~
## vcpkg setup for supported hosts. This file chooses a default vcpkg
## triplet when the user has not set one explicitly, then initializes the
## vcpkg toolchain after compiler detection.
## MinGW builds should use 'pacman' to install required dependency
## libraries.
##-=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~

if (NOT USING_VCPKG)
    return ()
endif ()

include(vcpkg-triplet)
zimh_configure_vcpkg_triplet()

## Set VCPKG_CRT_LINKAGE to pass down so that SIMH matches the triplet's link
## environment. Otherwise, the build will get a lot of "/NODEFAULTLIB" warnings.
set(VCPKG_CRT_LINKAGE "dynamic")
if (VCPKG_TARGET_TRIPLET MATCHES ".*-static")
    set(VCPKG_CRT_LINKAGE "static")
endif ()

message(STATUS "Executing deferred vcpkg toolchain initialization.\n"
    "    .. VCPKG target triplet is ${VCPKG_TARGET_TRIPLET}\n"
    "    .. VCPKG_CRT_LINKAGE is ${VCPKG_CRT_LINKAGE}")

## Initialize vcpkg after CMake detects the compiler and after we set the
## platform triplet. VCPKG_INSTALL_OPTIONS are additional args to
## 'vcpkg install'. Don't need to see the usage instructions each time.
if (NOT ("--no-print-usage" IN_LIST VCPKG_INSTALL_OPTIONS))
    list(APPEND VCPKG_INSTALL_OPTIONS
        "--no-print-usage"
    )
endif ()

include(${SIMH_CMAKE_TOOLCHAIN_FILE})
