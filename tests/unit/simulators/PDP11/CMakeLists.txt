add_unit_test(unit-pdp11-stddev
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_pdp11_stddev.c
        ${PROJECT_SOURCE_DIR}/simulators/PDP11/pdp11_stddev.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
    DEFINES
        VM_PDP11
)

add_unit_test(unit-pdp11-td-interrupts
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_pdp11_td_interrupts.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
    DEFINES
        VM_PDP11
)
