add_unit_test(unit-xalloc
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_xalloc.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)
