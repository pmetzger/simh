## AltairZ80 simulator
##
## Update this file as the local simulator sources and build metadata
## change. Keep shared target-construction logic in the common CMake helpers.
##
## Not yet: if this directory later gets a local unit-test subtree, declare
## it explicitly here with add_subdirectory(unit-tests).

zimh_find_bison(BISON_COMMAND BISON_JOB_POOL)

set(altairz80_musashi_source_dir "${PROJECT_SOURCE_DIR}/third_party/musashi")
set(altairz80_musashi_binary_dir "${CMAKE_CURRENT_BINARY_DIR}/musashi")
set(altairz80_m68kops_c "${altairz80_musashi_binary_dir}/m68kops.c")
set(altairz80_m68kops_h "${altairz80_musashi_binary_dir}/m68kops.h")

set(altairz80_m68kasm_c "${CMAKE_CURRENT_BINARY_DIR}/m68k/m68kasm.c")

add_executable(altairz80_m68kmake
    EXCLUDE_FROM_ALL
    "${altairz80_musashi_source_dir}/m68kmake.c"
)
set_target_properties(altairz80_m68kmake PROPERTIES
    C_STANDARD ${ZIMH_C_STANDARD}
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

add_custom_command(
    OUTPUT
        "${altairz80_m68kops_c}"
        "${altairz80_m68kops_h}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${altairz80_musashi_binary_dir}"
    COMMAND "$<TARGET_FILE:altairz80_m68kmake>"
        "${altairz80_musashi_binary_dir}"
        "${altairz80_musashi_source_dir}/m68k_in.c"
    DEPENDS
        altairz80_m68kmake
        "${altairz80_musashi_source_dir}/m68k_in.c"
    VERBATIM
)

add_custom_command(
    OUTPUT
        "${altairz80_m68kasm_c}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/m68k"
    COMMAND "${BISON_COMMAND}"
        -o "${altairz80_m68kasm_c}"
        "${CMAKE_CURRENT_SOURCE_DIR}/m68k/m68kasm.y"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/m68k/m68kasm.y"
    ${BISON_JOB_POOL}
    VERBATIM
)

add_simulator(altairz80
    SOURCES
        altairz80_cpu.c
        altairz80_cpu_nommu.c
        s100_tuart.c
        s100_dazzler.c
        s100_jair.c
        sol20.c
        s100_vdm1.c
        mmd.c
        s100_dj2d.c
        s100_djhdc.c
        altairz80_dsk.c
        disasm.c
        altairz80_sio.c
        altairz80_sys.c
        altairz80_hdsk.c
        altairz80_net.c
        s100_hayes.c
        s100_2sio.c
        s100_pmmi.c
        flashwriter2.c
        i86_decode.c
        i86_ops.c
        i86_prim_ops.c
        i8272.c
        insnsd.c
        altairz80_mhdsk.c
        ibc.c
        ibc_mcc_hdc.c
        ibc_smd_hdc.c
        mfdc.c
        n8vem.c
        vfdhd.c
        s100_disk1a.c
        s100_disk2.c
        s100_disk3.c
        s100_fif.c
        s100_mdriveh.c
        s100_icom.c
        s100_jadedd.c
        s100_mdsa.c
        s100_mdsad.c
        s100_selchan.c
        s100_ss1.c
        s100_64fdc.c
        s100_scp300f.c
        s100_tarbell.c
        s100_tdd.c
        wd179x.c
        s100_hdc1001.c
        s100_if3.c
        s100_adcs6.c
        "${altairz80_musashi_source_dir}/m68kcpu.c"
        "${altairz80_musashi_source_dir}/m68kdasm.c"
        ${altairz80_m68kasm_c}
        "${altairz80_musashi_source_dir}/softfloat/softfloat.c"
        "${altairz80_m68kops_c}"
        m68ksim.c
    DEFINES
        MUSASHI_CNF=\"m68kconf_zimh.h\"
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${altairz80_musashi_binary_dir}
        ${altairz80_musashi_source_dir}
    FEATURE_VIDEO
    LABEL AltairZ80
    PKG_FAMILY altairz80_family
    TEST altairz80)

set(altairz80_boot_rom_choice_output_dir
    "${CMAKE_CURRENT_BINARY_DIR}/tests/altairz80-boot-rom-choice")
set(altairz80_wd179x_index_pulse_output_dir
    "${CMAKE_CURRENT_BINARY_DIR}/tests/altairz80-wd179x-index-pulse")

find_package(Python3 COMPONENTS Interpreter)
if (Python3_Interpreter_FOUND)
    add_test(
        NAME zimh-altairz80-boot-rom-choice
        COMMAND
            ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/boot_rom_choice_test.py"
            $<TARGET_FILE:altairz80>
            --work-dir
            "${altairz80_boot_rom_choice_output_dir}")

    set_tests_properties(
        zimh-altairz80-boot-rom-choice
        PROPERTIES
            LABELS "zimh;integration;zimh-AltairZ80"
            ENVIRONMENT
            "SIMH_TEST_OUTPUT_DIR=${altairz80_boot_rom_choice_output_dir}")

    add_test(
        NAME zimh-altairz80-wd179x-index-pulse
        COMMAND
            ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/wd179x_index_pulse_test.py"
            $<TARGET_FILE:altairz80>
            --work-dir
            "${altairz80_wd179x_index_pulse_output_dir}")

    set_tests_properties(
        zimh-altairz80-wd179x-index-pulse
        PROPERTIES
            LABELS "zimh;integration;zimh-AltairZ80"
            ENVIRONMENT
            "SIMH_TEST_OUTPUT_DIR=${altairz80_wd179x_index_pulse_output_dir}")
endif ()
