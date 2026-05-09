#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "pdp8_defs.h"
#include "test_scp_fixture.h"
#include "test_simh_personality.h"
#include "test_support.h"

#include "pdp8_td.c"

uint16_t M[MAXMEMSIZE];

void cpu_set_bootpc(int32_t pc)
{
    (void)pc;
}

t_stat set_dev(UNIT *uptr, int32_t val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;
    return SCPE_OK;
}

t_stat show_dev(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;
    return SCPE_OK;
}

struct pdp8_td_fixture {
    char temp_dir[1024];
    char file_path[1024];
};

static int setup_pdp8_td_fixture(void **state)
{
    struct pdp8_td_fixture *fixture;
    DEVICE *devices[2];

    fixture = calloc(1, sizeof(*fixture));
    assert_non_null(fixture);

    assert_int_equal(simh_test_make_temp_dir(fixture->temp_dir,
                                             sizeof(fixture->temp_dir),
                                             "pdp8-td"),
                     0);
    assert_int_equal(simh_test_join_path(fixture->file_path,
                                         sizeof(fixture->file_path),
                                         fixture->temp_dir, "td0.dt"),
                     0);

    devices[0] = &td_dev;
    devices[1] = NULL;
    assert_int_equal(simh_test_install_devices("zimh-unit-pdp8-td", devices),
                     0);

    for (int unit = 0; unit < DT_NUMDR; ++unit) {
        sim_cancel(&td_unit[unit]);
        if ((td_unit[unit].flags & UNIT_ATT) != 0)
            assert_int_equal(td_detach(&td_unit[unit]), SCPE_OK);

        td_unit[unit].flags =
            UNIT_8FMT | UNIT_FIX | UNIT_ATTABLE | UNIT_DISABLE | UNIT_ROABLE;
        td_unit[unit].filebuf = NULL;
        td_unit[unit].hwmark = 0;
        td_unit[unit].capac = DT_CAPAC;
        td_unit[unit].pos = 0;
        td_unit[unit].STATE = 0;
        td_unit[unit].LASTT = 0;
        td_unit[unit].WRITTEN = false;
    }

    td_cmd = 0;
    td_dat = 0;
    td_mtk = 0;
    td_slf = 0;
    td_qlf = 0;
    td_tme = 0;
    td_csum = 0;
    td_qlctr = 0;

    *state = fixture;
    return 0;
}

static int teardown_pdp8_td_fixture(void **state)
{
    struct pdp8_td_fixture *fixture = *state;

    for (int unit = 0; unit < DT_NUMDR; ++unit) {
        sim_cancel(&td_unit[unit]);
        if ((td_unit[unit].flags & UNIT_ATT) != 0)
            assert_int_equal(td_detach(&td_unit[unit]), SCPE_OK);
        free(td_unit[unit].filebuf);
        td_unit[unit].filebuf = NULL;
        td_unit[unit].flags &= ~UNIT_BUF;
    }

    if (fixture != NULL) {
        assert_int_equal(simh_test_remove_path(fixture->temp_dir), 0);
        free(fixture);
    }
    *state = NULL;
    return 0;
}

/*
 * Detaching a TD8E image releases the memory buffer and returns the unit to
 * the detached 12-bit-format defaults expected by a later attach. This
 * characterizes the behavior guarded while removing stale local bookkeeping.
 */
static void test_td_detach_releases_buffer_and_restores_defaults(void **state)
{
    struct pdp8_td_fixture *fixture = *state;
    UNIT *unit = &td_unit[0];

    sim_switches = SWMASK('N');
    assert_int_equal(attach_unit(unit, fixture->file_path), SCPE_OK);

    unit->filebuf = calloc(DT_CAPAC, sizeof(uint16_t));
    assert_non_null(unit->filebuf);
    unit->flags = (unit->flags | UNIT_BUF | UNIT_11FMT) & ~UNIT_8FMT;
    unit->capac = D18_CAPAC;
    unit->pos = DT_EZLIN + 10;
    unit->STATE = STA_UTS | STA_DIR;
    unit->hwmark = 0;

    assert_int_equal(td_detach(unit), SCPE_OK);

    assert_true((unit->flags & UNIT_ATT) == 0);
    assert_true((unit->flags & UNIT_BUF) == 0);
    assert_true((unit->flags & UNIT_8FMT) != 0);
    assert_true((unit->flags & UNIT_11FMT) == 0);
    assert_null(unit->filebuf);
    assert_null(unit->fileref);
    assert_null(unit->filename);
    assert_int_equal(unit->capac, DT_CAPAC);
    assert_int_equal(unit->pos, 0);
    assert_int_equal(unit->STATE, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_td_detach_releases_buffer_and_restores_defaults,
            setup_pdp8_td_fixture, teardown_pdp8_td_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
