# =============================================================================
# simh-dependencies.cmake
# Consolidated Dependency Resolution and Interface Linking
# =============================================================================

# Establish Dependency "Buckets" (Interface Targets). Simulators link
# to these unconditionally.
add_library(simh_network INTERFACE)
add_library(simh_video INTERFACE)
add_library(simh_regexp INTERFACE)

# =============================================================================
# Function: find_vcpkg_pkgconfig_target
# Description: Safely extracts multi-config Release/Debug paths from vcpkg's
#              pkg-config files and wraps them in an IMPORTED INTERFACE target.
# =============================================================================
function(find_vcpkg_pkgconfig_target pkg_prefix pc_filename out_target_var)
    find_package(PkgConfig QUIET)
    if(NOT PKG_CONFIG_FOUND)
        return()
    endif()

    # 1. Back up the original PKG_CONFIG_PATH
    set(BACKUP_PKG_CONFIG_PATH "$ENV{PKG_CONFIG_PATH}")

    # 2. Extract Release configurations
    set(ENV{PKG_CONFIG_PATH} "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/pkgconfig")
    pkg_check_modules(${pkg_prefix}_REL QUIET ${pc_filename})

    # 3. Extract Debug configurations
    set(ENV{PKG_CONFIG_PATH} "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib/pkgconfig")
    pkg_check_modules(${pkg_prefix}_DBG QUIET ${pc_filename})

    # 4. Restore the environment safely
    set(ENV{PKG_CONFIG_PATH} "${BACKUP_PKG_CONFIG_PATH}")

    # 5. If both are found, build the generator-expression wrapped target
    if(${pkg_prefix}_REL_FOUND AND ${pkg_prefix}_DBG_FOUND)
        set(target_name "vcpkg::${pkg_prefix}")

        # Only create the target if it doesn't already exist
        if(NOT TARGET ${target_name})
            add_library(${target_name} INTERFACE IMPORTED)

            # Wire up Include Directories
            target_include_directories(${target_name} INTERFACE
                "$<$<NOT:$<CONFIG:Debug>>:${${pkg_prefix}_REL_INCLUDE_DIRS}>"
                "$<$<CONFIG:Debug>:${${pkg_prefix}_DBG_INCLUDE_DIRS}>"
            )

            # Wire up Library Directories
            target_link_directories(${target_name} INTERFACE
                "$<$<NOT:$<CONFIG:Debug>>:${${pkg_prefix}_REL_LIBRARY_DIRS}>"
                "$<$<CONFIG:Debug>:${${pkg_prefix}_DBG_LIBRARY_DIRS}>"
            )

            # Wire up Link Libraries (Using absolute paths to prevent poisoning)
            target_link_libraries(${target_name} INTERFACE
                "$<$<NOT:$<CONFIG:Debug>>:${${pkg_prefix}_REL_LINK_LIBRARIES}>"
                "$<$<CONFIG:Debug>:${${pkg_prefix}_DBG_LINK_LIBRARIES}>"
            )
        endif()

        # 6. Export the target name back to the calling scope
        set(${out_target_var} ${target_name} PARENT_SCOPE)
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Networking Capabilities
# -----------------------------------------------------------------------------
if(WITH_NETWORK)
    # -----------------------------------------------------------------------------
    # PCAP Discovery (Strict Package Manager Pipeline)
    #
    # Note: This will be headers-only on Windows, full library linkage
    # everywhere else.
    # -----------------------------------------------------------------------------
    if(WIN32)
        # 1. Windows / vcpkg Pipeline
        # vcpkg intercepts find_package(PCAP) and provides its own internal target mapping
        find_package(PCAP REQUIRED)

        if(TARGET PCAP::PCAP)
            # PCAP on Windows (Npcap/WinPcap) historically only needs headers linked
            # for the simulator cores, but linking the target natively handles ws2_32
            target_compile_definitions(simh_network INTERFACE
                HAVE_PCAP_NETWORK
            )
            target_link_libraries(simh_network INTERFACE PCAP::PCAP)
            message(STATUS "Network: PCAP configured via vcpkg (Target Linked).")
        endif()
    else()
        # 2. POSIX Pipeline (Linux, FreeBSD, macOS)
        # Rely cleanly on pkg-config
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(PCAP_PKG QUIET IMPORTED_TARGET libpcap)

        if(TARGET PkgConfig::PCAP_PKG)
            target_compile_definitions(simh_network INTERFACE HAVE_PCAP_NETWORK)
            target_link_libraries(simh_network INTERFACE PkgConfig::PCAP_PKG)
            message(STATUS "Network: PCAP configured via pkg-config.")
        else()
            # Absolute fallback if a weird Linux distro doesn't ship libpcap.pc
            find_package(PCAP REQUIRED)
            if(TARGET PCAP::PCAP)
                target_compile_definitions(simh_network INTERFACE HAVE_PCAP_NETWORK)
                target_link_libraries(simh_network INTERFACE PCAP::PCAP)
            endif()
        endif()
    endif()

    # SLIRP + GLIB2
    # Default: We don' have libslirp.
    set(HAVE_LIBSLIRP FALSE)
    if(WITH_SLIRP)
        if(WIN32)
            # Windows / vcpkg / Multi-Config Pipeline:
            # # FindPkgConfig ignores multi-config and hardcodes the first paths it finds.
            # # We must fetch both configurations explicitly to prevent path-poisoning.
            # find_package(PkgConfig QUIET)

            # if(PKG_CONFIG_FOUND)
            #     # 1. Back up the original PKG_CONFIG_PATH
            #     set(BACKUP_PKG_CONFIG_PATH "$ENV{PKG_CONFIG_PATH}")

            #     # 2. Force pkg-config to extract Release paths
            #     set(ENV{PKG_CONFIG_PATH} "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/pkgconfig")
            #     pkg_check_modules(SLIRP_REL QUIET slirp)
            #     pkg_check_modules(GLIB2_REL QUIET glib-2.0)

            #     # 3. Force pkg-config to extract Debug paths
            #     set(ENV{PKG_CONFIG_PATH} "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib/pkgconfig")
            #     pkg_check_modules(SLIRP_DBG QUIET slirp)
            #     pkg_check_modules(GLIB2_DBG QUIET glib-2.0)

            #     # 4. Restore the environment
            #     set(ENV{PKG_CONFIG_PATH} "${BACKUP_PKG_CONFIG_PATH}")

            #     if(SLIRP_REL_FOUND AND GLIB2_REL_FOUND)
            #         # 5. Create a unified, multi-config aware INTERFACE library
            #         add_library(vcpkg::slirp_glib INTERFACE IMPORTED)

            #         # Wire up Include Directories
            #         target_include_directories(vcpkg::slirp_glib INTERFACE
            #             "$<$<NOT:$<CONFIG:Debug>>:${SLIRP_REL_INCLUDE_DIRS};${GLIB2_REL_INCLUDE_DIRS}>"
            #             "$<$<CONFIG:Debug>:${SLIRP_DBG_INCLUDE_DIRS};${GLIB2_DBG_INCLUDE_DIRS}>"
            #         )

            #         # Wire up Library Directories
            #         target_link_directories(vcpkg::slirp_glib INTERFACE
            #             "$<$<NOT:$<CONFIG:Debug>>:${SLIRP_REL_LIBRARY_DIRS};${GLIB2_REL_LIBRARY_DIRS}>"
            #             "$<$<CONFIG:Debug>:${SLIRP_DBG_LIBRARY_DIRS};${GLIB2_DBG_LIBRARY_DIRS}>"
            #         )

            #         # Wire up Link Libraries (Absolute paths from _LINK_LIBRARIES prevent poisoning)
            #         target_link_libraries(vcpkg::slirp_glib INTERFACE
            #             "$<$<NOT:$<CONFIG:Debug>>:${SLIRP_REL_LINK_LIBRARIES};${GLIB2_REL_LINK_LIBRARIES}>"
            #             "$<$<CONFIG:Debug>:${SLIRP_DBG_LINK_LIBRARIES};${GLIB2_DBG_LINK_LIBRARIES}>"
            #         )

            #         target_link_libraries(simh_network INTERFACE vcpkg::slirp_glib)
            #         target_compile_definitions(simh_network INTERFACE
            #             HAVE_SLIRP_NETWORK
            #             USE_SIMH_SLIRP_DEBUG
            #             LIBSLIRP_STATIC
            #         )
            #         set(HAVE_LIBSLIRP TRUE)
            #         message(STATUS "Network: NAT(SLiRP) configured via multi-config pkg-config.")
            #     endif()
            # endif()
            # Windows / vcpkg / Multi-Config Pipeline:
            # Use the helper to generate robust multi-config targets.

            find_vcpkg_pkgconfig_target(slirp slirp SLIRP_TARGET)
            find_vcpkg_pkgconfig_target(glib2 glib-2.0 GLIB2_TARGET)

            if(SLIRP_TARGET AND GLIB2_TARGET)
                # Link both distinct targets to the network interface
                target_link_libraries(simh_network INTERFACE
                    ${SLIRP_TARGET}
                    ${GLIB2_TARGET}
                )
                target_compile_definitions(simh_network INTERFACE
                    HAVE_SLIRP_NETWORK
                    USE_SIMH_SLIRP_DEBUG
                )
                if (VCPKG_TARGET_TRIPLET MATCHES "-static$")
                    target_compile_definitions(simh_network INTERFACE LIBSLIRP_STATIC)
                endif ()
                
                set(HAVE_LIBSLIRP TRUE)
                message(STATUS "Network: NAT(SLiRP) configured via multi-config helpers.")
            endif()
        else()
            # POSIX Pipeline (Linux, FreeBSD, macOS)
            # Fall back to standard system pkgconf tracking where single-config paths are safe.
            find_package(PkgConfig QUIET)
            if(PKG_CONFIG_FOUND)
                pkg_check_modules(GLIB2 QUIET glib-2.0)
                pkg_check_modules(SLIRP QUIET slirp)

                if(SLIRP_FOUND AND GLIB2_FOUND)
                    target_include_directories(simh_network INTERFACE ${SLIRP_INCLUDE_DIRS} ${GLIB2_INCLUDE_DIRS})
                    target_link_libraries(simh_network INTERFACE ${SLIRP_LIBRARIES} ${GLIB2_LIBRARIES})
                    if(SLIRP_LIBRARY_DIRS)
                        target_link_directories(simh_network INTERFACE ${SLIRP_LIBRARY_DIRS})
                    endif()
                    if(GLIB2_LIBRARY_DIRS)
                        target_link_directories(simh_network INTERFACE ${GLIB2_LIBRARY_DIRS})
                    endif()
                    target_compile_definitions(simh_network INTERFACE
                        HAVE_SLIRP_NETWORK
                        USE_SIMH_SLIRP_DEBUG
                    )
                    set(HAVE_LIBSLIRP TRUE)
                    message(STATUS "Network: NAT(SLiRP) configured via pkg-config.")
                endif()
            endif()
        endif()
    endif()

    # VDE (Non-Windows)
    if(WITH_VDE AND NOT WIN32)
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(VDE QUIET vdeplug)
            if(VDE_FOUND)
                target_compile_definitions(simh_network INTERFACE HAVE_VDE_NETWORK)
                target_link_libraries(simh_network INTERFACE ${VDE_LIBRARIES})
                target_include_directories(simh_network INTERFACE ${VDE_INCLUDE_DIRS})
                if(VDE_LIBRARY_DIRS)
                    target_link_directories(simh_network INTERFACE ${VDE_LIBRARY_DIRS})
                endif()
                message(STATUS "Network: VDE configured.")
            endif()
        endif()
    endif()

    # Set the defines so that the network code gets compiled.
    if (NOT WIN32
        AND (HAVE_TAP_NETWORK OR VDE_FOUND OR HAVE_LIBSLIRP OR TARGET PCAP::PCAP))
        # POSIX/Unixen environments
        target_compile_definitions(simh_network INTERFACE USE_NETWORK)
    elseif (WIN32 AND (HAVE_LIBSLIRP OR TARGET PCAP::PCAP))
        ## FIXME: Rethink the USE_LOADED_WINPCAP on the WIN32 path to enable
        ## network code.
        target_compile_definitions(simh_network INTERFACE USE_LOADED_WINPCAP)
    endif ()
endif()

# -----------------------------------------------------------------------------
# Video & Graphics
# -----------------------------------------------------------------------------
if(WITH_VIDEO)
    ## Can only enable video iff SDL2 has been found:
    find_package(SDL2 OPTIONAL_COMPONENTS SDL2main NAMES sdl2 SDL2)
    if (TARGET SDL2::SDL2-static OR TARGET SDL2::SDL2)
        find_package(PNG REQUIRED)
        find_package(Freetype REQUIRED)
        find_package(SDL2_ttf REQUIRED)

        target_compile_definitions(simh_video INTERFACE USE_SIM_VIDEO HAVE_LIBSDL)

        if (TARGET PNG::PNG)
            target_compile_definitions(simh_video INTERFACE HAVE_LIBPNG)
        endif ()
        if (TARGET FreeType::Freetype)
            ## Not entirely sure this will actually be used -- it's really for SDL2_ttf
            ## link time.
            target_compile_definitions(simh_video INTERFACE HAVE_FREETYPE)
        endif ()

        target_link_libraries(simh_video INTERFACE
            # Prefer static on Windows if it exists; otherwise default to shared/standard everywhere else
            $<IF:$<AND:$<BOOL:${WIN32}>,$<TARGET_EXISTS:SDL2::SDL2-static>>,SDL2::SDL2-static,SDL2::SDL2>
            $<IF:$<AND:$<BOOL:${WIN32}>,$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf-static>>,SDL2_ttf::SDL2_ttf-static,SDL2_ttf::SDL2_ttf>

            PNG::PNG
            Freetype::Freetype
        )

        message(STATUS "Video: PNG, Freetype, SDL2, and SDL2_ttf configured.")
    else ()
        message(STATUS "Video: SDL2 library not found, video not enabled.")
    endif ()
endif()

# -----------------------------------------------------------------------------
# Regular Expressions
# -----------------------------------------------------------------------------
if(WITH_REGEXP)
    find_package(PCRE2 REQUIRED)
    target_link_libraries(simh_regexp INTERFACE PCRE2::PCRE2)
    message(STATUS "Regex: PCRE2 configured.")
endif()
