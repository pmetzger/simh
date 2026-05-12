add_unit_test(unit-hp3000-lp
    LABEL HP3000
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_hp3000_lp.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/HP3000
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-hp3000-format
    LABEL HP3000
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_hp3000_format.c
        ${PROJECT_SOURCE_DIR}/simulators/HP3000/hp3000_format.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/HP3000
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)
