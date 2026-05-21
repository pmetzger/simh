add_unit_test(unit-sim-fio
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_fio.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sim-host-path
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_host_path.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sim-tempfile
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_tempfile.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sim-uuid
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_uuid.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sim-card
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_card.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sim-imd
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_imd.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sim-tape
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_tape.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sim-disk
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_disk.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sim-timer
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_timer.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sim-rom-patch
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_rom_patch.c
        ${PROJECT_SOURCE_DIR}/src/runtime/sim_rom_patch.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sim-scsi
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_scsi.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sim-time
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_time.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-uint-bits
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_uint_bits.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/src/runtime
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sim-frontpanel
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_frontpanel.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sim-sock
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_sock.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

add_unit_test(unit-sim-console
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_console.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

if (WITH_NETWORK)
    add_unit_test(unit-sim-ether
        LABEL unit
        DEFINES
            USE_READER_THREAD
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_ether.c
        INCLUDES
            ${PROJECT_SOURCE_DIR}/tests/unit/support
    )

    add_unit_test(unit-sim-ether-test-backend
        LABEL unit
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_ether_test_backend.c
        INCLUDES
            ${PROJECT_SOURCE_DIR}/tests/unit/support
    )
endif ()

if (WITH_NETWORK AND WITH_SLIRP)
    add_unit_test(unit-sim-slirp
        LABEL unit
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_slirp.c
        INCLUDES
            ${PROJECT_SOURCE_DIR}/tests/unit/support
    )
    # sim_slirp.c lives in the slirp archive but calls back into simhcore.
    # Strict one-pass static linkers need simhcore repeated after slirp.
    target_link_libraries(zimh-unit-sim-slirp PRIVATE slirp simhcore)
endif ()

add_unit_test(unit-sim-video
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_video.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)

if (WITH_VIDEO)
    add_unit_test(unit-sim-video-sdl
        LABEL unit
        FEATURE_INT64
        FEATURE_VIDEO
        BESM6_SDL_HACK
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_video_sdl.c
        INCLUDES
            ${PROJECT_SOURCE_DIR}/tests/unit/support
    )
endif ()

add_unit_test(unit-sim-tmxr
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_tmxr.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)
