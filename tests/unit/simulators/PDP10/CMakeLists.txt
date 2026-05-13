add_unit_test(unit-pdp10-ks-dup
    LABEL unit
    FEATURE_INT64
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_ks10_dup.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/PDP10
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
        ${PROJECT_SOURCE_DIR}/tests/unit/support
    DEFINES
        KS=1
)

add_unit_test(unit-pdp10-imp-ftp
    LABEL unit
    FEATURE_INT64
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_kx10_imp_ftp.c
        ${PROJECT_SOURCE_DIR}/simulators/PDP10/kx10_imp_ftp.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/PDP10
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)
