# =============================================================================
# FindPTW.cmake (Unified Manifest-Aware Threading Wrapper)
# =============================================================================
# Simplifies threading targeting across vcpkg (Windows) and native toolchains
# (Linux, FreeBSD, macOS). Maps everything to the unified 'PTW::PTW' target.
# =============================================================================

if(TARGET PTW::PTW)
    return()
endif()

set(PTW_FOUND FALSE)

# Windows Pathway: Resolve vcpkg's 'pthreads' port target
if(WIN32)
    find_package(PThreads4W QUIET)
    if(TARGET PThreads4W::PThreads4W)
        add_library(PTW::PTW INTERFACE IMPORTED)
        target_link_libraries(PTW::PTW INTERFACE PThreads4W::PThreads4W)
        set(PTW_FOUND TRUE)
    endif()
endif()

# POSIX Pathway: Resolve native system threads (or alternative setups)
if(NOT PTW_FOUND)
    find_package(Threads QUIET)
    if(TARGET Threads::Threads)
        add_library(PTW::PTW INTERFACE IMPORTED)
        target_link_libraries(PTW::PTW INTERFACE Threads::Threads)
        set(PTW_FOUND TRUE)
    endif()
endif()

# Hard-Stop Configuration Failure
if(NOT PTW_FOUND)
    message(FATAL_ERROR 
        "\n[CMake FATAL ERROR] A mandatory execution dependency is missing!\n"
        " Could not locate a valid threading environment.\n"
        "  - On Windows: Verify vcpkg has downloaded and compiled the 'pthreads' package.\n"
        "  - On POSIX: Ensure system pthread development headers/libraries are installed.\n"
    )
endif()

set(PTW_FOUND TRUE CACHE INTERNAL "Threading library available status")
