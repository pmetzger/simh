add_unit_test(unit-vax4xx-dz
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax4xx_dz.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax4xx_dz.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/VAX
    DEFINES
        VM_VAX
        VAX_440
        VAX_48
    FEATURE_FULL64
)

add_unit_test(unit-vax4xx-rz94
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax4xx_rz94.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax4xx_rz94.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/VAX
    DEFINES
        VM_VAX
        VAX_440
        VAX_48
    FEATURE_FULL64
)

add_unit_test(unit-vax-va-dma
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax_va_dma.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/VAX
    DEFINES
        VM_VAX
        VAX_630
    FEATURE_FULL64
)

add_unit_test(unit-vax-va-dma-behavior
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax_va_dma_behavior.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax_va.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/VAX
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
    DEFINES
        VM_VAX
        VAX_630
    FEATURE_FULL64
)

add_unit_test(unit-vax750-mem
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax750_mem.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/VAX
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
    DEFINES
        VM_VAX
        VAX_750
    FEATURE_FULL64
)

add_unit_test(unit-vax780-fload
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax780_fload.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax780_fload.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/VAX
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
    DEFINES
        VM_VAX
        VAX_780
    FEATURE_FULL64
)
