set(SIMH_UNIT_SUPPORT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_fixture.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_support.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_simh_personality.c
)

# The simulator personality stubs define symbols whose types depend on
# USE_INT64 and USE_ADDR64. Build one support library for each simulator core
# width so LTO sees one consistent type for every external symbol.
function(add_simh_unit_support_library target core_library)
    add_library(${target} STATIC ${SIMH_UNIT_SUPPORT_SOURCES})

    set_target_properties(${target} PROPERTIES
        C_STANDARD ${ZIMH_C_STANDARD}
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )

    target_include_directories(${target} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${SIMH_COMPAT_ROOT}
        ${SIMH_CORE_ROOT}
        ${SIMH_INCLUDE_ROOT}
        ${SIMH_RUNTIME_ROOT}
    )

    target_compile_definitions(${target} PRIVATE
        SIMH_TEST_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
        SIMH_TEST_BINARY_ROOT="${CMAKE_BINARY_DIR}"
    )

    target_link_libraries(${target} PUBLIC
        os_features
        ${core_library}
    )
endfunction ()

add_simh_unit_support_library(simh_unit_support simhcore)
add_simh_unit_support_library(simh_unit_support_i64 simhi64)
add_simh_unit_support_library(simh_unit_support_z64 simhz64)
