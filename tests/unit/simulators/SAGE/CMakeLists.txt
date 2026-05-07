add_unit_test(unit-sage-i8272
    LABEL SAGE
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_i8272.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/SAGE
        ${PROJECT_SOURCE_DIR}/tests/unit/support
    DEFINES
        HAVE_INT64
)
