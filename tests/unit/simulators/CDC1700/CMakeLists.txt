add_unit_test(unit-cdc1700-strings
    LABEL CDC1700
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_cdc1700_strings.c
        ${PROJECT_SOURCE_DIR}/simulators/CDC1700/cdc1700_dis.c
        ${PROJECT_SOURCE_DIR}/src/runtime/sim_string.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/CDC1700
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)
