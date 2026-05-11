add_unit_test(unit-vt11-stack
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vt11_stack.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vt11_source.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/components/display
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)
