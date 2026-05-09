#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "test_scp_expect_fixture.h"
#include "scp.h"
#include "sim_console.h"
#include "test_scp_fixture.h"
#include "test_simh_personality.h"
#include "test_support.h"

/* Return one SEND context to a clean empty state between tests. */
static void reset_send_context(SEND *snd)
{
    sim_send_clear(snd);
    free(snd->buffer);
    snd->buffer = NULL;
    snd->bufsize = 0;
    sim_send_init_context(snd, snd->dptr, snd->dbit);
}

/* Return one EXPECT context to its default typed halt state. */
static void reset_expect_context(EXPECT *exp)
{
    exp->default_haltafter = 0;
}

/* Clear the published EXPECT match variables from SCP substitution state. */
static void clear_expect_match_vars(void)
{
    sim_sub_var_clear_prefix("_EXPECT_MATCH_");
}

/* Build a fresh SCP/TMXR fixture and reset global SEND/EXPECT state. */
int simh_test_setup_scp_expect_fixture(void **state)
{
    struct scp_expect_fixture *fixture;
    DEVICE *devices[2];
    size_t i;

    fixture = calloc(1, sizeof(*fixture));
    assert_non_null(fixture);

    simh_test_init_device_unit(&fixture->device, &fixture->unit, "TTY", "TTY0",
                               0, 0, 8, 1);

    devices[0] = &fixture->device;
    devices[1] = NULL;
    assert_int_equal(simh_test_install_devices("zimh-unit-scp-expect", devices),
                     0);

    sim_expect_init_context(&fixture->exp, &fixture->device, 0);
    sim_send_init_context(&fixture->snd, &fixture->device, 0);
    fixture->mux.dptr = &fixture->device;
    fixture->mux.uptr = &fixture->unit;
    fixture->mux.ldsc = fixture->lines;
    fixture->mux.lines = 2;
    for (i = 0; i < 2; ++i) {
        fixture->lines[i].mp = &fixture->mux;
        fixture->lines[i].dptr = &fixture->device;
        fixture->lines[i].uptr = &fixture->unit;
        fixture->lines[i].o_uptr = &fixture->unit;
        fixture->lines[i].rxbsz = 16;
        fixture->lines[i].txbsz = 16;
    }
    assert_int_equal(tmxr_attach_ex(&fixture->mux, &fixture->unit,
                                    "LINE=0,LOOPBACK,LINE=1,LOOPBACK", false),
                     SCPE_OK);
    assert_int_equal(sim_brk_init(), SCPE_OK);
    sim_switches = 0;
    sim_switch_number = 0;
    clear_expect_match_vars();
    assert_int_equal(sim_exp_clrall(sim_cons_get_expect()), SCPE_OK);
    reset_send_context(sim_cons_get_send());
    reset_expect_context(sim_cons_get_expect());

    *state = fixture;
    return 0;
}

/* Tear down the SCP/TMXR fixture and clear any published test state. */
int simh_test_teardown_scp_expect_fixture(void **state)
{
    struct scp_expect_fixture *fixture = *state;
    size_t i;

    sim_exp_clrall(&fixture->exp);
    sim_send_clear(&fixture->snd);
    free(fixture->snd.buffer);
    fixture->snd.buffer = NULL;
    reset_expect_context(&fixture->exp);
    reset_send_context(&fixture->snd);
    for (i = 0; i < 2; ++i) {
        sim_exp_clrall(&fixture->lines[i].expect);
        reset_expect_context(&fixture->lines[i].expect);
        reset_send_context(&fixture->lines[i].send);
    }
    assert_int_equal(tmxr_detach(&fixture->mux, &fixture->unit), SCPE_OK);
    assert_int_equal(sim_exp_clrall(sim_cons_get_expect()), SCPE_OK);
    reset_send_context(sim_cons_get_send());
    reset_expect_context(sim_cons_get_expect());
    sim_brk_clract();
    clear_expect_match_vars();
    simh_test_reset_simulator_state();
    free(fixture);
    *state = NULL;
    return 0;
}
