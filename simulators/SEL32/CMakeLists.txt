## SEL32 simulator
##
## Update this file as the local simulator sources and build metadata
## change. Keep shared target-construction logic in the common CMake helpers.
##
## Not yet: if this directory later gets a local unit-test subtree, declare
## it explicitly here with add_subdirectory(unit-tests).

add_simulator(sel32
    SOURCES
        sel32_cpu.c
        sel32_sys.c
        sel32_chan.c
        sel32_iop.c
        sel32_com.c
        sel32_con.c
        sel32_clk.c
        sel32_mt.c
        sel32_lpr.c
        sel32_scfi.c
        sel32_fltpt.c
        sel32_disk.c
        sel32_hsdp.c
        sel32_mfp.c
        sel32_scsi.c
        sel32_ec.c
        sel32_ipu.c
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src/runtime
    DEFINES
        USE_INT32
        SEL32
    LABEL SEL32
    PKG_FAMILY gould_family
    TEST sel32)
