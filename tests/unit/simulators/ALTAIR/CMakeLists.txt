add_unit_test(unit-altair-dsk
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_altair_dsk.c
        ${PROJECT_SOURCE_DIR}/simulators/ALTAIR/altair_dsk.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/ALTAIR
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

# This target includes the real Altair system personality, so it cannot use
# add_unit_test(), which supplies generic simulator personality stubs.
simh_executable_template(zimh-unit-altair-sys
    SOURCES
        ${PROJECT_SOURCE_DIR}/tests/unit/support/test_support.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test_altair_sys.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/ALTAIR
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

target_link_libraries(zimh-unit-altair-sys PUBLIC
    ${unittest_cmocka_target}
)
target_compile_definitions(zimh-unit-altair-sys PRIVATE
    SIMH_TEST_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    SIMH_TEST_BINARY_ROOT="${CMAKE_BINARY_DIR}"
)
add_test(NAME zimh-unit-altair-sys COMMAND zimh-unit-altair-sys)
set_tests_properties(zimh-unit-altair-sys PROPERTIES
    LABELS "zimh;unit;zimh-unit")
set_property(GLOBAL APPEND PROPERTY SIMH_UNIT_TEST_TARGETS
    zimh-unit-altair-sys)
