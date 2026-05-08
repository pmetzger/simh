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

add_unit_test(unit-pdp11-xq
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_pdp11_xq.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
    DEFINES
        VM_PDP11
)

add_unit_test(unit-vax-xq
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_pdp11_xq.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
        ${PROJECT_SOURCE_DIR}/simulators/VAX
    DEFINES
        VM_VAX
    FEATURE_FULL64
)
