add_unit_test(unit-sds-mux
    LABEL SDS
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sds_mux.c
        ${PROJECT_SOURCE_DIR}/simulators/SDS/sds_mux.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/SDS
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)
