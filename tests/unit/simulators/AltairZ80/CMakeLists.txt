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
