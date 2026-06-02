## IBM 1130 utility build checks.
##
## These utility programs are not installed and are not part of the default
## build.  Build the ibm1130-utils target explicitly to verify that the legacy
## utility sources still compile.

set(IBM1130_UTILITY_COMMON_SOURCES
    util_io.c)

set(IBM1130_UTILITY_NAMES
    asm1130
    bindump
    checkdisk
    disklist
    diskview
    mkboot
    punches
    viewdeck)

set(IBM1130_UTILITY_TARGETS)

foreach (utility IN LISTS IBM1130_UTILITY_NAMES)
    set(target "ibm1130-${utility}")
    add_executable(${target} EXCLUDE_FROM_ALL
        ${utility}.c
        ${IBM1130_UTILITY_COMMON_SOURCES}
        ${SIMH_COMPAT_SOURCES})
    set_target_properties(${target} PROPERTIES
        C_STANDARD ${CMAKE_C_STANDARD}
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
        OUTPUT_NAME ${utility})
    target_compile_options(${target} PRIVATE ${EXTRA_TARGET_CFLAGS})
    target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANG_AND_ID:C,AppleClang,Clang,GNU>:-Wno-missing-braces>)
    target_link_options(${target} PRIVATE ${EXTRA_TARGET_LFLAGS})
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${SIMH_COMPAT_ROOT}
        ${SIMH_CORE_ROOT}
        ${SIMH_INCLUDE_ROOT}
        ${SIMH_RUNTIME_ROOT}
        ${ZIMH_GENERATED_INCLUDE_DIR})
    target_link_libraries(${target} PRIVATE os_features)
    list(APPEND IBM1130_UTILITY_TARGETS ${target})
endforeach ()

add_custom_target(ibm1130-utils
    DEPENDS ${IBM1130_UTILITY_TARGETS})
