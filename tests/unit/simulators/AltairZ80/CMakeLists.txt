add_unit_test(unit-altairz80-dazzler
    LABEL AltairZ80
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_s100_dazzler.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/AltairZ80
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-altairz80-scp300f
    LABEL AltairZ80
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_s100_scp300f.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/AltairZ80
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-altairz80-softfloat-milieu
    LABEL AltairZ80
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_softfloat_milieu.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/AltairZ80/m68k/softfloat
)

add_unit_test(unit-altairz80-m68k-bool-namespace
    LABEL AltairZ80
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_m68k_bool_namespace.c
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/simulators/AltairZ80
        ${PROJECT_SOURCE_DIR}/simulators/AltairZ80/m68k
)

add_unit_test(unit-altairz80-flashwriter2
    LABEL AltairZ80
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_flashwriter2_attach.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/AltairZ80
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-altairz80-disasm
    LABEL AltairZ80
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_disasm.c
        ${PROJECT_SOURCE_DIR}/simulators/AltairZ80/disasm.c
        ${PROJECT_SOURCE_DIR}/simulators/AltairZ80/insnsd.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/AltairZ80
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

# This target includes the real AltairZ80 system personality, so it cannot use
# add_unit_test(), which supplies generic simulator personality stubs.
simh_executable_template(zimh-unit-altairz80-sys
    SOURCES
        ${PROJECT_SOURCE_DIR}/tests/unit/support/test_support.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test_altairz80_sys.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/AltairZ80
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

target_link_libraries(zimh-unit-altairz80-sys PUBLIC
    ${unittest_cmocka_target}
)
target_compile_definitions(zimh-unit-altairz80-sys PRIVATE
    SIMH_TEST_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    SIMH_TEST_BINARY_ROOT="${CMAKE_BINARY_DIR}"
)
add_test(NAME zimh-unit-altairz80-sys COMMAND zimh-unit-altairz80-sys)
set_tests_properties(zimh-unit-altairz80-sys PROPERTIES
    LABELS "zimh;unit;zimh-unit;zimh-AltairZ80")
set_property(GLOBAL APPEND PROPERTY SIMH_UNIT_TEST_TARGETS
    zimh-unit-altairz80-sys)
