add_unit_test(unit-h316-imp-switches
    LABEL H316
    DEFINES
        VM_IMPTIP
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_h316_imp_switches.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/H316
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-h316-hi
    LABEL H316
    DEFINES
        VM_IMPTIP
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_h316_hi.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/H316
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)
