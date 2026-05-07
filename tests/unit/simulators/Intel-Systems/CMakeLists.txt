# This target links the real i8080 sim_load(), so it cannot use
# add_unit_test(), which supplies generic simulator personality stubs.
simh_executable_template(zimh-unit-intel-systems-i8080-load
    SOURCES
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/common/i8080_load.c
        ${PROJECT_SOURCE_DIR}/tests/unit/support/test_support.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test_i8080_load.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/common
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/Intel-MDS
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

target_compile_definitions(zimh-unit-intel-systems-i8080-load PRIVATE
    SIMH_TEST_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    SIMH_TEST_BINARY_ROOT="${CMAKE_BINARY_DIR}"
)

if (TARGET cmocka::cmocka)
    target_link_libraries(zimh-unit-intel-systems-i8080-load
        PUBLIC cmocka::cmocka)
elseif (TARGET PkgConfig::CMOCKA)
    target_link_libraries(zimh-unit-intel-systems-i8080-load
        PUBLIC PkgConfig::CMOCKA)
else ()
    message(FATAL_ERROR "cmocka target not available")
endif ()
add_test(NAME zimh-unit-intel-systems-i8080-load
    COMMAND zimh-unit-intel-systems-i8080-load)
set_tests_properties(zimh-unit-intel-systems-i8080-load PROPERTIES
    LABELS "zimh;unit;zimh-intel-systems")
set_property(GLOBAL APPEND PROPERTY SIMH_UNIT_TEST_TARGETS
    zimh-unit-intel-systems-i8080-load)

simh_executable_template(zimh-unit-intel-systems-i8080-symbols
    SOURCES
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/common/i8080_symbol.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test_i8080_symbols.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/common
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/Intel-MDS
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

if (TARGET cmocka::cmocka)
    target_link_libraries(zimh-unit-intel-systems-i8080-symbols
        PUBLIC cmocka::cmocka)
elseif (TARGET PkgConfig::CMOCKA)
    target_link_libraries(zimh-unit-intel-systems-i8080-symbols
        PUBLIC PkgConfig::CMOCKA)
else ()
    message(FATAL_ERROR "cmocka target not available")
endif ()
add_test(NAME zimh-unit-intel-systems-i8080-symbols
    COMMAND zimh-unit-intel-systems-i8080-symbols)
set_tests_properties(zimh-unit-intel-systems-i8080-symbols PROPERTIES
    LABELS "zimh;unit;zimh-intel-systems")
set_property(GLOBAL APPEND PROPERTY SIMH_UNIT_TEST_TARGETS
    zimh-unit-intel-systems-i8080-symbols)

# This target includes the real i8008 CPU personality, so it cannot use
# add_unit_test(), which supplies generic simulator personality stubs.
simh_executable_template(zimh-unit-intel-systems-i8008-symbolic
    SOURCES
        ${PROJECT_SOURCE_DIR}/tests/unit/support/test_support.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test_i8008_symbolic.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/common
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/scelbi
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

target_link_libraries(zimh-unit-intel-systems-i8008-symbolic PUBLIC
    ${unittest_cmocka_target}
)
target_compile_definitions(zimh-unit-intel-systems-i8008-symbolic PRIVATE
    SIMH_TEST_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    SIMH_TEST_BINARY_ROOT="${CMAKE_BINARY_DIR}"
)
add_test(NAME zimh-unit-intel-systems-i8008-symbolic
    COMMAND zimh-unit-intel-systems-i8008-symbolic)
set_tests_properties(zimh-unit-intel-systems-i8008-symbolic PROPERTIES
    LABELS "zimh;unit;zimh-intel-systems")
set_property(GLOBAL APPEND PROPERTY SIMH_UNIT_TEST_TARGETS
    zimh-unit-intel-systems-i8008-symbolic)

add_unit_test(unit-intel-systems-iram8
    SOURCES
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/common/iram8.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test_iram8.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/common
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/Intel-MDS
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/tests/unit/support
    LABEL intel-systems)

add_unit_test(unit-intel-systems-isbc201
    SOURCES
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/common/isbc201.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test_isbc201.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/common
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/Intel-MDS
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/tests/unit/support
    LABEL intel-systems)

add_unit_test(unit-intel-systems-isbc202
    SOURCES
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/common/isbc202.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test_isbc202.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/common
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/Intel-MDS
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/tests/unit/support
    LABEL intel-systems)

add_unit_test(unit-intel-systems-isbc206
    SOURCES
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/common/isbc206.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test_isbc206.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/common
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/Intel-MDS
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/tests/unit/support
    LABEL intel-systems)

add_unit_test(unit-intel-systems-zx200a
    SOURCES
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/common/zx200a.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test_zx200a.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/common
        ${PROJECT_SOURCE_DIR}/simulators/Intel-Systems/Intel-MDS
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/tests/unit/support
    LABEL intel-systems)
