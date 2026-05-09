#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "test_cmocka.h"

#include "sigma_io_defs.h"

extern UNIT dk_unit[];
uint32_t dk_tio_status(uint32_t un);

uint32_t chan_ctl_time = 0;

uint32_t chan_get_cmd(uint32_t dva, uint32_t *cmd)
{
    (void)dva;
    (void)cmd;
    return SCPE_IERR;
}

uint32_t chan_set_chf(uint32_t dva, uint32_t fl)
{
    (void)dva;
    (void)fl;
    return SCPE_IERR;
}

int32_t chan_clr_chi(uint32_t dva)
{
    (void)dva;
    return -1;
}

bool chan_chk_dvi(uint32_t dva)
{
    (void)dva;
    return false;
}

uint32_t chan_end(uint32_t dva)
{
    (void)dva;
    return SCPE_IERR;
}

uint32_t chan_uen(uint32_t dva)
{
    (void)dva;
    return SCPE_IERR;
}

uint32_t chan_RdMemB(uint32_t dva, uint32_t *dat)
{
    (void)dva;
    (void)dat;
    return SCPE_IERR;
}

uint32_t chan_WrMemB(uint32_t dva, uint32_t dat)
{
    (void)dva;
    (void)dat;
    return SCPE_IERR;
}

uint32_t chan_RdMemW(uint32_t dva, uint32_t *dat)
{
    (void)dva;
    (void)dat;
    return SCPE_IERR;
}

uint32_t chan_WrMemW(uint32_t dva, uint32_t dat)
{
    (void)dva;
    (void)dat;
    return SCPE_IERR;
}

t_stat chan_reset_dev(uint32_t dva)
{
    (void)dva;
    return SCPE_IERR;
}

t_stat io_set_dvc(UNIT *uptr, int32_t val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;
    return SCPE_IERR;
}

t_stat io_show_dvc(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;
    return SCPE_IERR;
}

t_stat io_set_dva(UNIT *uptr, int32_t val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;
    return SCPE_IERR;
}

t_stat io_show_dva(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;
    return SCPE_IERR;
}

t_stat io_show_cst(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;
    return SCPE_IERR;
}

static void cancel_dk_units(void)
{
    for (uint32_t i = 0; i < 8; i++)
        sim_cancel(&dk_unit[i]);
}

static int setup_sigma_dk(void **state)
{
    (void)state;
    cancel_dk_units();
    return 0;
}

static int teardown_sigma_dk(void **state)
{
    (void)state;
    cancel_dk_units();
    return 0;
}

static void assert_controller_busy(uint32_t status)
{
    assert_int_equal(status & DVS_CBUSY, DVS_CBUSY);
    assert_int_equal(status & (CC2 << DVT_V_CC), CC2 << DVT_V_CC);
}

static void test_dk_tio_status_reports_unit_zero_busy(void **state)
{
    (void)state;

    assert_int_equal(sim_activate_abs(&dk_unit[0], 100), SCPE_OK);

    assert_controller_busy(dk_tio_status(0));
}

static void test_dk_tio_status_reports_nonzero_unit_busy(void **state)
{
    (void)state;

    assert_int_equal(sim_activate_abs(&dk_unit[1], 100), SCPE_OK);

    assert_controller_busy(dk_tio_status(0));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_dk_tio_status_reports_unit_zero_busy, setup_sigma_dk,
            teardown_sigma_dk),
        cmocka_unit_test_setup_teardown(
            test_dk_tio_status_reports_nonzero_unit_busy, setup_sigma_dk,
            teardown_sigma_dk),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
