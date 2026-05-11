## Maintained directly.
##
## Keep non-simulator developer/sample utility targets here. Shared simulator
## target construction belongs in add_simulator.cmake.

## Stub simulator skeleton.
##
## This is a developer/sample target, not part of the normal simulator
## inventory or automated test suite.
simh_executable_template(stub
    SOURCES
        ${SIMH_COMPONENTS_ROOT}/stub/stub_sys.c
        ${SIMH_COMPONENTS_ROOT}/stub/stub_cpu.c
    INCLUDES
        ${SIMH_COMPONENTS_ROOT}/stub)
target_sources(stub PRIVATE ${SIMH_CORE_ROOT}/main.c)

## Front panel test sample.
##
## From all evidence in Makefile, sim_frontpanel is not yet used by any
## simulator targets.
add_executable(frontpaneltest
    ${SIMH_COMPONENTS_ROOT}/frontpanel/FrontPanelTest.c
    ${SIMH_RUNTIME_ROOT}/sim_sock.c
    ${SIMH_RUNTIME_ROOT}/sim_string.c
    ${SIMH_RUNTIME_ROOT}/sim_time.c
    ${SIMH_RUNTIME_ROOT}/sim_frontpanel.c)

target_include_directories(frontpaneltest PUBLIC
    "${SIMH_COMPAT_ROOT}"
    "${SIMH_CORE_ROOT}"
    "${SIMH_INCLUDE_ROOT}"
    "${SIMH_RUNTIME_ROOT}"
    "${SIMH_COMPONENTS_ROOT}")
target_link_libraries(frontpaneltest PUBLIC os_features thread_lib)

if (WIN32)
    target_link_libraries(frontpaneltest PUBLIC simh_network)

    if (MSVC)
        target_link_options(frontpaneltest PUBLIC "/SUBSYSTEM:CONSOLE")
    elseif (MINGW)
        target_link_options(frontpaneltest PUBLIC "-mconsole")
    endif ()
endif (WIN32)

## Developer/sample utilities that are useful to keep buildable but are not
## part of the default simulator or test workflows.
add_custom_target(extra-tools
    DEPENDS
        frontpaneltest
        stub)
