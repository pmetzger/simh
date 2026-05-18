set(SIMH_COMPAT_STRING_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_compat_string.c
    ${SIMH_COMPAT_ROOT}/strlcpy.c
    ${SIMH_COMPAT_ROOT}/strlcat.c
    ${SIMH_COMPAT_ROOT}/strdup.c
    ${SIMH_COMPAT_ROOT}/strndup.c
    ${SIMH_COMPAT_ROOT}/strcasecmp.c
    ${SIMH_COMPAT_ROOT}/strncasecmp.c)

set(SIMH_COMPAT_STRING_DEFINES
    SIMH_NEED_STRLCPY
    SIMH_NEED_STRLCAT
    SIMH_NEED_STRDUP
    SIMH_NEED_STRNDUP
    SIMH_NEED_STRCASECMP
    SIMH_NEED_STRNCASECMP)

if (SIMH_NEED_STRNLEN)
    list(APPEND SIMH_COMPAT_STRING_SOURCES ${SIMH_COMPAT_ROOT}/strnlen.c)
    list(APPEND SIMH_COMPAT_STRING_DEFINES SIMH_NEED_STRNLEN)
endif ()

add_unit_test(unit-compat-string
    LABEL unit
    SOURCES ${SIMH_COMPAT_STRING_SOURCES}
    DEFINES ${SIMH_COMPAT_STRING_DEFINES}
)

if (WIN32)
    add_unit_test(unit-compat-time
        LABEL unit
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/test_compat_time.c
    )

    add_unit_test(unit-compat-env
        LABEL unit
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/test_compat_env.c
    )
endif ()
