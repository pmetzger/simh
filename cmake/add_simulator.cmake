## Put everything together into one nice function.

include (CTest)

## Simulator sources and library:
set(SIM_SOURCES
    ${SIMH_CORE_ROOT}/scp.c
    ${SIMH_CORE_ROOT}/scp_breakpoint.c
    ${SIMH_CORE_ROOT}/scp_cmdvars.c
    ${SIMH_CORE_ROOT}/scp_context.c
    ${SIMH_CORE_ROOT}/scp_expect.c
    ${SIMH_CORE_ROOT}/scp_expr.c
    ${SIMH_CORE_ROOT}/scp_help_engine.c
    ${SIMH_CORE_ROOT}/scp_parse.c
    ${SIMH_CORE_ROOT}/scp_unit.c
    ${SIMH_RUNTIME_ROOT}/sim_card.c
    ${SIMH_RUNTIME_ROOT}/sim_console.c
    ${SIMH_RUNTIME_ROOT}/sim_disk.c
    ${SIMH_RUNTIME_ROOT}/sim_disk_ramdisk.c
    ${SIMH_RUNTIME_ROOT}/sim_dynstr.c
    ${SIMH_RUNTIME_ROOT}/sim_ether.c
    ${SIMH_RUNTIME_ROOT}/sim_fio.c
    ${SIMH_RUNTIME_ROOT}/sim_host_path.c
    ${SIMH_RUNTIME_ROOT}/sim_imd.c
    ${SIMH_RUNTIME_ROOT}/sim_scsi.c
    ${SIMH_RUNTIME_ROOT}/sim_serial.c
    ${SIMH_RUNTIME_ROOT}/sim_sock.c
    ${SIMH_RUNTIME_ROOT}/sim_tape.c
    ${SIMH_RUNTIME_ROOT}/sim_tape_testlib.c
    ${SIMH_RUNTIME_ROOT}/sim_tempfile.c
    ${SIMH_RUNTIME_ROOT}/sim_time.c
    ${SIMH_RUNTIME_ROOT}/sim_timer.c
    ${SIMH_RUNTIME_ROOT}/sim_tmxr.c
    ${SIMH_RUNTIME_ROOT}/sim_video.c)

if (SIMH_COMPAT_SOURCES)
    list(APPEND SIM_SOURCES
        ${SIMH_COMPAT_SOURCES})
endif ()

set(SIM_VIDEO_SOURCES
    ${SIMH_COMPONENTS_ROOT}/display/display.c
    ${SIMH_COMPONENTS_ROOT}/display/sim_ws.c)

function(zimh_find_bison command_var job_pool_args_var)
    find_program(ZIMH_BISON_COMMAND
        NAMES bison win_bison
        REQUIRED)

    get_filename_component(_zimh_bison_name "${ZIMH_BISON_COMMAND}" NAME_WE)
    set(_zimh_bison_job_pool_args)

    if (_zimh_bison_name STREQUAL "win_bison")
        # win_bison.exe uses process-global temporary output names, so parallel
        # invocations can collide. Keep those custom commands serialized.
        get_property(_zimh_job_pools GLOBAL PROPERTY JOB_POOLS)
        list(FIND _zimh_job_pools "win_bison=1" _zimh_win_bison_pool)
        if (_zimh_win_bison_pool EQUAL -1)
            set_property(GLOBAL APPEND PROPERTY JOB_POOLS win_bison=1)
        endif ()
        set(_zimh_bison_job_pool_args JOB_POOL win_bison)
    endif ()

    set(${command_var} "${ZIMH_BISON_COMMAND}" PARENT_SCOPE)
    set(${job_pool_args_var} "${_zimh_bison_job_pool_args}" PARENT_SCOPE)
endfunction()

## Build a simulator core library, with and without AIO support. The AIO variant
## has "_aio" appended to its name, e.g., "simhz64_aio" or "simhz64_video_aio".
function(build_simcore _targ)
    cmake_parse_arguments(SIMH "VIDEO;INT64;ADDR64;BESM6_SDL_HACK" "" "" ${ARGN})

    # Additional library targets that depend on simulator I/O:
    add_library(${_targ} STATIC ${SIM_SOURCES})

    set(sim_aio_lib "${_targ}_aio")
    add_library(${sim_aio_lib} STATIC ${SIM_SOURCES})

    # Components that need to be turned on while building the library, but
    # don't export out to the dependencies (hence PRIVATE.)
    foreach (lib IN ITEMS "${_targ}" "${sim_aio_lib}")
        set_target_properties(${lib} PROPERTIES
            C_STANDARD ${ZIMH_C_STANDARD}
            C_STANDARD_REQUIRED ON
            C_EXTENSIONS OFF
            EXCLUDE_FROM_ALL True
        )
        target_compile_definitions(${lib} PRIVATE USE_SIM_CARD USE_SIM_IMD)
        target_compile_options(${lib} PRIVATE ${EXTRA_TARGET_CFLAGS})
        target_link_options(${lib} PRIVATE ${EXTRA_TARGET_LFLAGS})

        # Make sure the shared source roots are part of the library include path.
        target_include_directories("${lib}" PUBLIC
            "${SIMH_COMPAT_ROOT}"
            "${SIMH_CORE_ROOT}"
            "${SIMH_RUNTIME_ROOT}"
            "${SIMH_COMPONENTS_ROOT}"
            "${ZIMH_GENERATED_INCLUDE_DIR}")

        if (SIMH_INT64)
            target_compile_definitions(${lib} PUBLIC USE_INT64)
        endif (SIMH_INT64)

        if (SIMH_ADDR64)
            target_compile_definitions(${lib} PUBLIC USE_ADDR64)
        endif (SIMH_ADDR64)

        if (SIMH_VIDEO)
            if (WITH_VIDEO)
                # It's the video library
                target_sources(${lib} PRIVATE ${SIM_VIDEO_SOURCES})
                target_link_libraries(${lib} PUBLIC simh_video)
            endif ()
            if (CMAKE_HOST_APPLE AND NOT SIMH_BESM6_SDL_HACK)
                ## (a) The BESM6 SDL hack is temporary. If SDL_MAIN_AVAILABLE needs
                ##     to be defined, it belongs in the simh_video interface library.
                ## (b) BESM6 doesn't use SIMH's video capabilities correctly and
                ##     the Makefile filters out SDL_MAIN_AVAILABLE on macOS.
                ## (c) This shouldn't be just an Apple platform quirk; SDL_main should
                ##     be used by all platforms. <sigh!>
                target_compile_definitions("${lib}" PUBLIC SDL_MAIN_AVAILABLE)
            endif ()
        endif ()

        # Define SIM_BUILD_TOOL for the simulator'
        target_compile_definitions("${lib}" PRIVATE
             "SIM_BUILD_TOOL=CMake (${CMAKE_GENERATOR})"
        )

        target_link_libraries(${lib} PUBLIC
            simh_network
            simh_regexp
            os_features
            thread_lib
        )

    endforeach ()

    target_compile_definitions(${sim_aio_lib} PUBLIC ${AIO_FLAGS})

    # Create target cppcheck rule, if detected.
    if (ENABLE_CPPCHECK AND cppcheck_cmd)
        get_property(cppcheck_includes TARGET ${_targ} PROPERTY INCLUDE_DIRECTORIES)
        get_property(cppcheck_defines  TARGET ${_targ} PROPERTY COMPILE_DEFINITIONS)
        list(TRANSFORM cppcheck_includes PREPEND "-I")
        list(TRANSFORM cppcheck_defines  PREPEND "-D")

        add_custom_target("${_targ}_cppcheck"
            COMMAND ${cppcheck_cmd} ${cppcheck_defines} ${cppcheck_includes} ${SIM_SOURCES}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            VERBATIM
        )

        add_dependencies(cppcheck "${_targ}_cppcheck")
    endif (ENABLE_CPPCHECK AND cppcheck_cmd)
endfunction(build_simcore _targ)


##+
## Basic template for simh executables and unit tests.
##-
## FEATURE_INT64: Use the simhi64 library (defines USE_INT64)
## FEATURE_FULL64: Use the simhz64 library (defines USE_INT64, USE_ADDR64)
## BUILDROMS: Build the hardcoded boot rooms
## FEATURE_VIDEO: Add video support
## FEATURE_DISPLAY: Add display support
##
## BESM6_SDL_HACK: A TEMPORARY feature to work around issues with BESM6 and
##   SDL and SIMH video. Because it's a day ending in "y", there's going to
##   be another Makefile quirk.
list(APPEND ADD_SIMULATOR_OPTIONS
    "FEATURE_INT64"
    "FEATURE_FULL64"
    "BUILDROMS"
    "FEATURE_VIDEO"
    "FEATURE_DISPLAY"
    "NO_INSTALL"
    "BESM6_SDL_HACK"
    "USES_AIO"
)

## TEST: The test script name that will be executed by the simulator within CTest.
## LABEL: The test name label, used to group tests, e.g., "VAX" for all of the
##   VAX simulator tests. If you want to run a subset of tests, add the "-L <regexp>"
##   argument to the ctest command line.
## PKG_FAMILY: The simulator family to which a simulator belongs. If not specificed,
##   defaults to "default_family".
list(APPEND ADD_SIMULATOR_1ARG
    "TEST"
    "LABEL"
    "PKG_FAMILY"
)

## DEFINES: List of extra command line manifest constants ("-D" items)
## INCLUDES: List of extra include directories
## SOURCES: List of source files
## TEST_ARGS: Additional arguments to append to the command line after
##   "RegisterSanityCheck"
list(APPEND ADD_SIMULATOR_NARG
    "DEFINES"
    "INCLUDES"
    "SOURCES"
    "TEST_ARGS"
)

function (simh_executable_template _targ)
    cmake_parse_arguments(SIMH "${ADD_SIMULATOR_OPTIONS}" "${ADD_SIMULATOR_1ARG}" "${ADD_SIMULATOR_NARG}" ${ARGN})

    if (NOT DEFINED SIMH_SOURCES)
        message(FATAL_ERROR "${_targ}: No source files?")
    endif (NOT DEFINED SIMH_SOURCES)

    if (SIMH_USES_AIO AND NOT WITH_ASYNC)
        message(WARNING
          "!!! ${_targ}: Asynchronous I/O not enabled, but this simulator specifies USES_AIO\n"
          "!!!           Some features will be crippled, notably networking.")
    endif ()

    add_executable("${_targ}" "${SIMH_SOURCES}")
    set_target_properties(${_targ} PROPERTIES
        C_STANDARD ${ZIMH_C_STANDARD}
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${SIMH_RUNTIME_OUTPUT_DIR}"
    )
    foreach (_cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER "${_cfg}" _cfg_uc)
        set_property(TARGET ${_targ}
            PROPERTY "RUNTIME_OUTPUT_DIRECTORY_${_cfg_uc}"
            "${SIMH_RUNTIME_OUTPUT_DIR}")
    endforeach ()
    target_compile_options(${_targ} PRIVATE ${EXTRA_TARGET_CFLAGS})
    target_link_options(${_targ} PRIVATE ${EXTRA_TARGET_LFLAGS})

    if (MINGW)
        ## target_compile_options(${_targ} PUBLIC "-fms-extensions")
        target_link_options(${_targ} PUBLIC "-mconsole")
    elseif (MSVC)
        target_link_options(${_targ} PUBLIC "/SUBSYSTEM:CONSOLE")
    endif ()

    if (DEFINED SIMH_DEFINES)
        target_compile_definitions("${_targ}" PUBLIC "${SIMH_DEFINES}")
    endif (DEFINED SIMH_DEFINES)

    # This is a quick cheat to make sure that all of the include paths are
    # absolute paths.
    if (DEFINED SIMH_INCLUDES)
        set(_normalized_includes)
        foreach (inc IN LISTS SIMH_INCLUDES)
            if (NOT IS_ABSOLUTE "${inc}")
                message(">> Fixing include for ${_targ}: ${inc} -> ${CMAKE_SOURCE_DIR}/${inc}")
                get_filename_component(inc "${CMAKE_SOURCE_DIR}/${inc}" ABSOLUTE)
            endif ()
            list(APPEND _normalized_includes "${inc}")
        endforeach ()

        target_include_directories("${_targ}" PUBLIC "${_normalized_includes}")
    endif ()

    if (WITH_VIDEO)
        if (SIMH_FEATURE_DISPLAY)
            target_compile_definitions(${_targ} PUBLIC USE_DISPLAY)
        endif ()
        if (CMAKE_HOST_APPLE
            AND (SIMH_FEATURE_VIDEO OR SIMH_FEATURE_DISPLAY)
            AND NOT SIMH_BESM6_SDL_HACK)
            target_compile_definitions(${_targ} PUBLIC SDL_MAIN_AVAILABLE)
        endif ()
    endif ()

    set(SIMH_SIMLIB simhcore)
    if (SIMH_FEATURE_INT64)
        set(SIMH_SIMLIB simhi64)
    elseif (SIMH_FEATURE_FULL64)
        set(SIMH_SIMLIB simhz64)
    endif ()

    if (SIMH_FEATURE_VIDEO OR SIMH_FEATURE_DISPLAY)
        if (NOT SIMH_BESM6_SDL_HACK)
            string(APPEND SIMH_SIMLIB "_video")
        else ()
            string(APPEND SIMH_SIMLIB "_besm6")
        endif ()
    endif ()

    # Uses AIO...
    if (SIMH_USES_AIO)
        set(SIMH_SIMLIB "${SIMH_SIMLIB}_aio")
    endif()

    target_link_libraries("${_targ}" PUBLIC "${SIMH_SIMLIB}")
endfunction ()


function (add_simulator _targ)
    simh_executable_template(${_targ} "${ARGN}")
    cmake_parse_arguments(SIMH "${ADD_SIMULATOR_OPTIONS}" "${ADD_SIMULATOR_1ARG}" "${ADD_SIMULATOR_NARG}" ${ARGN})

    target_sources(${_targ} PRIVATE ${SIMH_CORE_ROOT}/main.c)
    set_target_properties(${_targ} PROPERTIES OUTPUT_NAME "zimh-${_targ}")

    set(pkg_family "default_family")
    if (SIMH_PKG_FAMILY)
        set(pkg_family ${SIMH_PKG_FAMILY})
    endif ()

    set(_install_opts)
    if (SIMH_CMAKE_RUNTIME_DEPENDENCIES_SUPPORTED)
        list(APPEND _install_opts RUNTIME_DEPENDENCY_SET simhRuntime)
    endif ()

    install(TARGETS ${_targ}
        ${_install_opts}
        RUNTIME
        COMPONENT ${pkg_family}
    )

    if (SIMH_CMAKE_RUNTIME_DEPENDENCIES_SUPPORTED)
        ## Collect runtime dependencies. The simhRuntime install() is invoked by cpack-setup.cmake
        install(IMPORTED_RUNTIME_ARTIFACTS ${_targ} COMPONENT ${pkg_family} BUNDLE COMPONENT ${pkg_family})
    endif()

    ## Simulator-specific tests:
    list(APPEND test_cmd "${_targ}" "RegisterSanityCheck")
    if (SIMH_TEST_ARGS)
        list(APPEND test_cmd ${SIMH_TEST_ARGS})
    endif ()

    if (DEFINED SIMH_TEST)
        string(APPEND test_fname ${CMAKE_CURRENT_SOURCE_DIR} "/tests/${SIMH_TEST}_test.ini")
        IF (EXISTS "${test_fname}")
            list(APPEND test_cmd "${test_fname}" "-v")
        ENDIF ()
    endif ()

    add_test(NAME "zimh-${_targ}" COMMAND ${test_cmd})

    set(TEST_LABELS "zimh;integration")
    if (SIMH_LABEL)
        list(APPEND TEST_LABELS "zimh-${SIMH_LABEL}")
    endif ()
    set_tests_properties("zimh-${_targ}" PROPERTIES LABELS "${TEST_LABELS}")

    set(test_output_dir "${CMAKE_CURRENT_BINARY_DIR}/tests/${_targ}")
    file(MAKE_DIRECTORY "${test_output_dir}")
    file(TO_NATIVE_PATH "${test_output_dir}" native_test_output_dir)

    if (BUILD_SHARED_DEPS)
        ## Make sure that the tests can find the DLLs/shared objects:
        file(TO_NATIVE_PATH "${SIMH_DEP_TOPDIR}/bin" native_dep_bindir)
        file(TO_NATIVE_PATH "${SIMH_RUNTIME_OUTPUT_DIR}" native_binary_dir)
    endif ()

    ## Build up test environment:
    ## - Simulators that link SDL2 need to use the dummy (null) drivers on tests within
    ##   the CI/CD environment. While not STRICTLY necessary, it's a good idea.
    list(APPEND test_add_env "SDL_VIDEODRIVER=dummy")
    list(APPEND test_add_env "SDL_AUDIODRIVER=dummy")
    list(APPEND test_add_env
         "SIMH_TEST_OUTPUT_DIR=${native_test_output_dir}")

    if (WIN32)
        if (BUILD_SHARED_DEPS)
            set(test_path "PATH=${native_dep_bindir}\\\$<SEMICOLON>${native_binary_dir}\\$<SEMICOLON>")
            string(REPLACE ";" "\\\$<SEMICOLON>" escaped_path "$ENV{PATH}")
            string(APPEND test_path "${escaped_path}")
            list(APPEND test_add_env "${test_path}")
        endif ()
    else ()
        if (BUILD_SHARED_DEPS)
            list(APPEND test_add_env "LD_LIBRARY_PATH=${native_dep_bindir}:${native_binary_dir}:\$LD_LIBRARY_PATH")
        endif ()
    endif ()

    set_property(TEST "zimh-${_targ}" PROPERTY ENVIRONMENT "${test_add_env}")

    if (NOT WITH_ROMS)
        target_compile_definitions(${_targ} PRIVATE DONT_USE_INTERNAL_ROM)
    elseif (SIMH_BUILDROMS)
        add_dependencies(${_targ} BuildROMs)
    endif ()

    # Create target 'cppcheck' rule, if cppcheck detected:
    if (ENABLE_CPPCHECK AND cppcheck_cmd)
        get_property(cppcheck_includes TARGET ${_targ} PROPERTY INCLUDE_DIRECTORIES)
        get_property(cppcheck_defines  TARGET ${_targ} PROPERTY COMPILE_DEFINITIONS)
        list(TRANSFORM cppcheck_includes PREPEND "-I")
        list(TRANSFORM cppcheck_defines  PREPEND "-D")
        add_custom_target("${_targ}_cppcheck"
            COMMAND ${cppcheck_cmd} ${cppcheck_defines} ${cppcheck_includes} ${SIMH_SOURCES}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )

        add_dependencies(cppcheck "${_targ}_cppcheck")
    endif (ENABLE_CPPCHECK AND cppcheck_cmd)

    set_property(GLOBAL APPEND PROPERTY SIMH_INTEGRATION_TARGETS ${_targ})
endfunction ()


function(add_unit_test _targ)
    set(UNIT_TARGET "zimh-${_targ}")
    set(UNIT_TEST "zimh-${_targ}")

    simh_executable_template(${UNIT_TARGET} "${ARGN}")
    cmake_parse_arguments(SIMH "FEATURE_INT64;FEATURE_FULL64;BUILDROMS;FEATURE_VIDEO,FEATURE_DISPLAY"
                          "SOURCE_DIR;LABEL"
                          "DEFINES;INCLUDES;SOURCES"
                          ${ARGN})

    target_link_libraries(${UNIT_TARGET} PUBLIC unittest)
    add_test(NAME ${UNIT_TEST} COMMAND ${UNIT_TARGET})

    set(TEST_LABEL "zimh;unit")
    if (SIMH_LABEL)
      list(APPEND TEST_LABEL "zimh-${SIMH_LABEL}")
    endif ()
    set_tests_properties(${UNIT_TEST} PROPERTIES LABELS "${TEST_LABEL}")

    set_property(GLOBAL APPEND PROPERTY SIMH_UNIT_TEST_TARGETS ${UNIT_TARGET})
endfunction ()

##~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
## Now build things!
##~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=

if (ENABLE_CPPCHECK AND cppcheck_cmd)
    add_custom_target(cppcheck)
endif ()

build_simcore(simhcore)
build_simcore(simhi64        INT64)
build_simcore(simhz64        INT64 ADDR64)
build_simcore(simhcore_video VIDEO)
build_simcore(simhi64_video  VIDEO INT64)
build_simcore(simhz64_video  VIDEO INT64 ADDR64)

## Temporary hack for BESM6's breakage.
build_simcore(simhi64_besm6  VIDEO INT64 BESM6_SDL_HACK)

if (WITH_ROMS)
    add_executable(BuildROMs ${SIMH_RUNTIME_ROOT}/sim_BuildROMs.c)
    target_include_directories(BuildROMs PUBLIC
        "${SIMH_CORE_ROOT}"
        "${SIMH_RUNTIME_ROOT}")
    target_link_libraries(BuildROMs os_features)
    add_custom_command(
        OUTPUT
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka655x_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka620_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka630_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka610_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka410_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka411_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka412_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka41a_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka41d_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka42a_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka42b_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka43a_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka46a_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka47a_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka48a_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_is1000_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka410_xs_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka420_rdrz_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka420_rzrz_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka4xx_4pln_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka4xx_8pln_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka4xx_dz_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka4xx_spx_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka750_bin_new.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_ka750_bin_old.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_vcb02_bin.h
            ${SIMH_SIMULATOR_ROOT}/3B2/roms/3b2_400_bin.h
            ${SIMH_SIMULATOR_ROOT}/3B2/roms/3b2_700_bin.h
            ${SIMH_SIMULATOR_ROOT}/VAX/vax_vmb_exe.h
            ${SIMH_SIMULATOR_ROOT}/PDP11/pdp11_vt_lunar_rom.h
            ${SIMH_SIMULATOR_ROOT}/PDP11/pdp11_dazzle_dart_rom.h
            ${SIMH_SIMULATOR_ROOT}/PDP11/pdp11_11logo_rom.h
            ${SIMH_SIMULATOR_ROOT}/swtp6800/swtp6800/swtp_swtbugv10_bin.h
        MAIN_DEPENDENCY BuildROMs
        COMMAND BuildROMS
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif ()
