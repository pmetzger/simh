add_unit_test(unit-sigma-dk
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sigma_dk.c
        ${PROJECT_SOURCE_DIR}/simulators/sigma/sigma_dk.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/sigma
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sigma-dp
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sigma_dp.c
        ${PROJECT_SOURCE_DIR}/simulators/sigma/sigma_dp.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/sigma
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sigma-cis
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sigma_cis.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/sigma
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)
