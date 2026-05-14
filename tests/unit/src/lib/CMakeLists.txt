add_unit_test(unit-dynstr
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_dynstr.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/lib
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-xalloc
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_xalloc.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-string-util
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_string_util.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)
