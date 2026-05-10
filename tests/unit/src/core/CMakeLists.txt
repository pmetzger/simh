add_unit_test(unit-scp-size
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_size.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-scp-parse
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_parse.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-scp-unit
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_unit.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-scp-register
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_register.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-scp-context
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_context.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-scp-expr
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_expr.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-scp-cmdvars
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_cmdvars.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-scp-message
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_message.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sim-printf-fmts
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_printf_fmts.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sim-printf-fmts-i64
    LABEL unit
    FEATURE_INT64
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_printf_fmts.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sim-printf-fmts-z64
    LABEL unit
    FEATURE_FULL64
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_printf_fmts.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-scp-expect
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_expect.c
        ${PROJECT_SOURCE_DIR}/tests/unit/support/test_scp_expect_fixture.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-scp-regex
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_regex.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-scp-breakpoint
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_breakpoint.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-scp-bitfield
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_bitfield.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-scp-help-engine
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_help_engine.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)
