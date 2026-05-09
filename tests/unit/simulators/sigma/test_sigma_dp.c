#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "test_cmocka.h"

#include "sigma_io_defs.h"

extern DEVICE dp_dev[];

t_stat dp_set_ctl(UNIT *uptr, int32_t val, const char *cptr, void *desc);

uint32_t chan_ctl_time = 0;

t_stat io_boot(int32_t unitno, DEVICE *dptr)
{
    (void)unitno;
    (void)dptr;
    return SCPE_IERR;
}

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

bool chan_tst_cmf(uint32_t dva, uint32_t fl)
{
    (void)dva;
    (void)fl;
    return 0;
}

int32_t chan_chk_chi(uint32_t dva)
{
    (void)dva;
    return -1;
}

int32_t chan_clr_chi(uint32_t dva)
{
    (void)dva;
    return -1;
}

void chan_set_dvi(uint32_t dva)
{
    (void)dva;
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

static void test_dp_set_ctl_rejects_negative_controller_type(void **state)
{
    (void)state;

    assert_int_equal(dp_set_ctl(&dp_dev[0].units[0], -1, NULL, NULL),
                     SCPE_IERR);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_dp_set_ctl_rejects_negative_controller_type),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
