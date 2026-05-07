## Intel-Systems/Intel-MDS simulator
##
## Update this file as the local simulator sources and build metadata
## change. Keep shared target-construction logic in the common CMake helpers.
##
## Not yet: if this directory later gets a local unit-test subtree, declare
## it explicitly here with add_subdirectory(unit-tests).

add_simulator(intel-mds
    SOURCES
        ${INTELSYSC}/i8080.c
        ${INTELSYSC}/i8080_symbol.c
        ${INTELSYSC}/i8080_load.c
        imds_sys.c
        ${INTELSYSC}/i3214.c
        ${INTELSYSC}/i8251.c
        ${INTELSYSC}/i8253.c
        ${INTELSYSC}/i8255.c
        ${INTELSYSC}/i8259.c
        ${INTELSYSC}/ieprom.c
        ${INTELSYSC}/ioc-cont.c
        ${INTELSYSC}/ipc-cont.c
        ${INTELSYSC}/iram8.c
        ${INTELSYSC}/isbc064.c
        ${INTELSYSC}/isbc202.c
        ${INTELSYSC}/isbc201.c
        ${INTELSYSC}/isbc206.c
        ${INTELSYSC}/isbc464.c
        ${INTELSYSC}/isbc208.c
        ${INTELSYSC}/port.c
        ${INTELSYSC}/irq.c
        ${INTELSYSC}/multibus.c
        ${INTELSYSC}/mem.c
        ${INTELSYSC}/sys.c
        ${INTELSYSC}/zx200a.c
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
    BUILDROMS
    LABEL Intel-Systems
    PKG_FAMILY intel_family
    TEST intel-mds)
