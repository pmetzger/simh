add_unit_test(unit-imlac-load
    LABEL unit
    REAL_SIMH_PERSONALITY
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_imlac_load.c
        ${PROJECT_SOURCE_DIR}/simulators/imlac/imlac_sys.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/imlac
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)
