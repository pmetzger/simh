add_unit_test(unit-sel32-memory
    LABEL SEL32
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sel32_memory.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/SEL32
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sel32-channel
    LABEL SEL32
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sel32_channel.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/SEL32
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sel32-cpu
    LABEL SEL32
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sel32_cpu.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/SEL32
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sel32-clock
    LABEL SEL32
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sel32_clock.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/SEL32
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)
