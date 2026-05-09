add_library(simh_unit_support STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_fixture.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_support.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_simh_personality.c
)

set_target_properties(simh_unit_support PROPERTIES
    C_STANDARD ${ZIMH_C_STANDARD}
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

target_include_directories(simh_unit_support PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SIMH_COMPAT_ROOT}
    ${SIMH_CORE_ROOT}
    ${SIMH_INCLUDE_ROOT}
    ${SIMH_RUNTIME_ROOT}
)

target_compile_definitions(simh_unit_support PRIVATE
    SIMH_TEST_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    SIMH_TEST_BINARY_ROOT="${CMAKE_BINARY_DIR}"
)
