set(SIMH_UNIT_SUPPORT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_support.c
)

set(SIMH_UNIT_PERSONALITY_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_fixture.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_simh_personality.c
)

# Build one support object library for each simulator core width so tests get
# helper objects directly on their link line, without creating static archive
# ordering cycles against the simulator core libraries.
function(add_simh_unit_support_objects target core_library)
    add_library(${target} OBJECT ${SIMH_UNIT_SUPPORT_SOURCES})

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

    target_link_libraries(${target} PUBLIC os_features)

    if (core_library MATCHES "i64|z64")
        target_compile_definitions(${target} PRIVATE USE_INT64)
    endif ()
    if (core_library MATCHES "z64")
        target_compile_definitions(${target} PRIVATE USE_ADDR64)
    endif ()
endfunction ()

# The fake simulator personality stubs define symbols whose types depend on
# USE_INT64 and USE_ADDR64. Build matching object libraries so add_unit_test()
# can link the right fake personality objects directly. Tests that compile a
# real simulator personality opt out with REAL_SIMH_PERSONALITY.
function(add_simh_unit_personality_objects target core_library)
    add_library(${target} OBJECT ${SIMH_UNIT_PERSONALITY_SOURCES})

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

    target_link_libraries(${target} PRIVATE os_features)

    if (core_library MATCHES "i64|z64")
        target_compile_definitions(${target} PRIVATE USE_INT64)
    endif ()
    if (core_library MATCHES "z64")
        target_compile_definitions(${target} PRIVATE USE_ADDR64)
    endif ()
endfunction ()

add_simh_unit_support_objects(simh_unit_support simhcore)
add_simh_unit_support_objects(simh_unit_support_i64 simhi64)
add_simh_unit_support_objects(simh_unit_support_z64 simhz64)

add_simh_unit_personality_objects(simh_unit_personality simhcore)
add_simh_unit_personality_objects(simh_unit_personality_i64 simhi64)
add_simh_unit_personality_objects(simh_unit_personality_z64 simhz64)
