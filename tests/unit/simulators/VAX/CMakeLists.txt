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

add_unit_test(unit-vax4xx-stddev-behavior
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax4xx_stddev_behavior.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax4xx_rom_patch.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax4xx_stddev.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/simulators/VAX
    DEFINES
        VM_VAX
        VAX_440
        VAX_48
    FEATURE_FULL64
)

add_unit_test(unit-vax410-sysdev-behavior
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax4xx_sysdev_behavior.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax410_sysdev.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/simulators/VAX
    DEFINES
        DONT_USE_INTERNAL_ROM
        VM_VAX
        VAX_410
    FEATURE_FULL64
)

add_unit_test(unit-vax420-sysdev-behavior
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax4xx_sysdev_behavior.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax420_sysdev.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/simulators/VAX
    DEFINES
        DONT_USE_INTERNAL_ROM
        VM_VAX
        VAX_420
        VAX_42A
    FEATURE_FULL64
)

add_unit_test(unit-vax-sysdev-behavior
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax_sysdev_behavior.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax_sysdev.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/simulators/VAX
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
    DEFINES
        VM_VAX
    FEATURE_FULL64
)

add_unit_test(unit-vax43-sysdev-behavior
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax43_sysdev_behavior.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax43_sysdev.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/simulators/VAX
    DEFINES
        VM_VAX
        VAX_43
        VAX_43A
    FEATURE_FULL64
)

add_unit_test(unit-vax440-sysdev-behavior
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax440_sysdev_behavior.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax440_sysdev.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/simulators/VAX
    DEFINES
        DONT_USE_INTERNAL_ROM
        VM_VAX
        VAX_440
        VAX_48
    FEATURE_FULL64
)

add_unit_test(unit-vax630-rom-byte-write
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax_rom_byte_write_behavior.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax630_sysdev.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/simulators/VAX
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
    DEFINES
        TEST_VAX630_SYSDEV
        VM_VAX
        VAX_630
    FEATURE_FULL64
)

add_unit_test(unit-vax4nn-stddev-rom-byte-write
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax_rom_byte_write_behavior.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax4nn_stddev.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/simulators/VAX
    DEFINES
        VM_VAX
        IS_1000
    FEATURE_FULL64
)

add_unit_test(unit-vax-cpu1-call
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax_cpu1_call.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/VAX
    DEFINES
        VM_VAX
    FEATURE_FULL64
)

add_unit_test(unit-vax-qbus-mem-write
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax_qbus_mem_write.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax_io.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/simulators/VAX
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
    DEFINES
        VM_VAX
    FEATURE_FULL64
)

add_unit_test(unit-vax630-qbus-mem-write
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax_qbus_mem_write.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax630_io.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/simulators/VAX
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
    DEFINES
        VM_VAX
        VAX_630
    FEATURE_FULL64
)

add_unit_test(unit-vax-qbus-io-write
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax_qbus_io_write.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax_io.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/simulators/VAX
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
    DEFINES
        VM_VAX
        VAX_QBUS_TEST_RECORD_WRITES
    FEATURE_FULL64
)

add_unit_test(unit-vax-qbus-io-read
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax_qbus_io_read.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax_io.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/simulators/VAX
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
    DEFINES
        VM_VAX
        VAX_QBUS_TEST_RECORD_READS
    FEATURE_FULL64
)

add_unit_test(unit-vax610-qbus-io-write
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax_qbus_io_write.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax610_io.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/simulators/VAX
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
    DEFINES
        VM_VAX
        VAX_610
        VAX_QBUS_TEST_RECORD_WRITES
    FEATURE_FULL64
)

add_unit_test(unit-vax610-qbus-io-read
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax_qbus_io_read.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax610_io.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/simulators/VAX
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
    DEFINES
        VM_VAX
        VAX_610
        VAX_QBUS_TEST_RECORD_READS
    FEATURE_FULL64
)

add_unit_test(unit-vax630-qbus-io-write
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax_qbus_io_write.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax630_io.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/simulators/VAX
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
    DEFINES
        VM_VAX
        VAX_630
        VAX_QBUS_TEST_RECORD_WRITES
    FEATURE_FULL64
)

add_unit_test(unit-vax630-qbus-io-read
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax_qbus_io_read.c
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax630_io.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/simulators/VAX
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
    DEFINES
        VM_VAX
        VAX_630
        VAX_QBUS_TEST_RECORD_READS
    FEATURE_FULL64
)

simh_executable_template(zimh-unit-vax-symbols
    SOURCES
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax_sys.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test_vax_symbols.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/VAX
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/tests/unit/support
    FEATURE_FULL64
)

target_compile_definitions(zimh-unit-vax-symbols PRIVATE
    VM_VAX
    VAX_440
)

if (TARGET cmocka::cmocka)
    target_link_libraries(zimh-unit-vax-symbols PUBLIC cmocka::cmocka)
elseif (TARGET PkgConfig::CMOCKA)
    target_link_libraries(zimh-unit-vax-symbols PUBLIC PkgConfig::CMOCKA)
else ()
    message(FATAL_ERROR "cmocka target not available")
endif ()

add_test(NAME zimh-unit-vax-symbols COMMAND zimh-unit-vax-symbols)
set_tests_properties(zimh-unit-vax-symbols PROPERTIES
    LABELS "zimh;unit;zimh-VAX")
set_property(GLOBAL APPEND PROPERTY SIMH_UNIT_TEST_TARGETS
    zimh-unit-vax-symbols)

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
        ${PROJECT_SOURCE_DIR}/simulators/VAX/vax750_mem.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/simulators/VAX
        ${PROJECT_SOURCE_DIR}/simulators/PDP11
    DEFINES
        DONT_USE_INTERNAL_ROM
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
