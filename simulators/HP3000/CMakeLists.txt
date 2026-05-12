## HP3000 simulator
##
## Update this file as the local simulator sources and build metadata
## change. Keep shared target-construction logic in the common CMake helpers.
##
## Not yet: if this directory later gets a local unit-test subtree, declare
## it explicitly here with add_subdirectory(unit-tests).

add_simulator(hp3000
    SOURCES
        hp_disclib.c
        hp_tapelib.c
        hp3000_atc.c
        hp3000_clk.c
        hp3000_cpu.c
        hp3000_cpu_base.c
        hp3000_cpu_fp.c
        hp3000_cpu_cis.c
        hp3000_ds.c
        hp3000_format.c
        hp3000_iop.c
        hp3000_lp.c
        hp3000_mem.c
        hp3000_mpx.c
        hp3000_ms.c
        hp3000_scmb.c
        hp3000_sel.c
        hp3000_sys.c
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
    LABEL HP3000
    PKG_FAMILY hp_family
    TEST hp3000)
