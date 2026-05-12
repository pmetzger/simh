add_unit_test(unit-hp2100-sys
    LABEL HP2100
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_hp2100_sys.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/HP2100
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-hp2100-format
    LABEL HP2100
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_hp2100_format.c
        ${PROJECT_SOURCE_DIR}/simulators/HP2100/hp2100_format.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/HP2100
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)
