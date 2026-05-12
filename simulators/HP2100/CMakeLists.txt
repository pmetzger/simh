## HP2100 simulator
##
## Update this file as the local simulator sources and build metadata
## change. Keep shared target-construction logic in the common CMake helpers.
##
## Not yet: if this directory later gets a local unit-test subtree, declare
## it explicitly here with add_subdirectory(unit-tests).

add_simulator(hp2100
    SOURCES
        hp2100_baci.c
        hp2100_cpu.c
        hp2100_cpu_fp.c
        hp2100_cpu_fpp.c
        hp2100_cpu0.c
        hp2100_cpu1.c
        hp2100_cpu2.c
        hp2100_cpu3.c
        hp2100_cpu4.c
        hp2100_cpu5.c
        hp2100_cpu6.c
        hp2100_cpu7.c
        hp2100_di.c
        hp2100_di_da.c
        hp2100_disclib.c
        hp2100_dma.c
        hp2100_format.c
        hp2100_dp.c
        hp2100_dq.c
        hp2100_dr.c
        hp2100_ds.c
        hp2100_ipl.c
        hp2100_lps.c
        hp2100_lpt.c
        hp2100_mc.c
        hp2100_mem.c
        hp2100_mpx.c
        hp2100_ms.c
        hp2100_mt.c
        hp2100_mux.c
        hp2100_pif.c
        hp2100_pt.c
        hp2100_sys.c
        hp2100_tbg.c
        hp2100_tty.c
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
    DEFINES
        HAVE_INT64
    LABEL HP2100
    PKG_FAMILY hp_family
    TEST hp2100)
