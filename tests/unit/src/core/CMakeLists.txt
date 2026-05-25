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

add_executable(zimh-unit-scp-cmdvars-env-helper
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdvars_env_helper.c
)
set_target_properties(zimh-unit-scp-cmdvars-env-helper PROPERTIES
    C_STANDARD ${ZIMH_C_STANDARD}
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
target_compile_options(zimh-unit-scp-cmdvars-env-helper
    PRIVATE ${EXTRA_TARGET_CFLAGS}
)
target_link_options(zimh-unit-scp-cmdvars-env-helper
    PRIVATE ${EXTRA_TARGET_LFLAGS}
)
target_link_libraries(zimh-unit-scp-cmdvars-env-helper
    PRIVATE os_features
)

add_unit_test(unit-scp-cmdvars
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_cmdvars.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)
target_compile_definitions(zimh-unit-scp-cmdvars PRIVATE
    SIMH_CMDVARS_ENV_HELPER="$<TARGET_FILE:zimh-unit-scp-cmdvars-env-helper>"
)
add_dependencies(zimh-unit-scp-cmdvars zimh-unit-scp-cmdvars-env-helper)

add_unit_test(unit-scp-message
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_message.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-scp-format
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_format.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-scp-debug
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_debug.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-scp-input
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_input.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/core
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
