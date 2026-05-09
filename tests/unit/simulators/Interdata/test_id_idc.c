#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "test_cmocka.h"

#include "id_defs.h"

uint32 int_req[INTSZ];
uint32 int_enb[INTSZ];

int32 int_chg(uint32 irq, int32 dat, int32 armdis)
{
    (void)irq;
    (void)armdis;
    return dat;
}

void sch_adr(uint32 ch, uint32 dev)
{
    (void)ch;
    (void)dev;
}

bool sch_actv(uint32 sch, uint32 devno)
{
    (void)sch;
    (void)devno;
    return false;
}

void sch_stop(uint32 sch)
{
    (void)sch;
}

uint32 sch_wrmem(uint32 sch, uint8 *buf, uint32 cnt)
{
    (void)sch;
    (void)buf;
    return cnt;
}

uint32 sch_rdmem(uint32 sch, uint8 *buf, uint32 cnt)
{
    (void)sch;
    (void)buf;
    return cnt;
}

t_stat set_sch(UNIT *uptr, int32 val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;
    return SCPE_OK;
}

t_stat set_dev(UNIT *uptr, int32 val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;
    return SCPE_OK;
}

t_stat show_sch(FILE *st, UNIT *uptr, int32 val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;
    return SCPE_OK;
}

t_stat show_dev(FILE *st, UNIT *uptr, int32 val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;
    return SCPE_OK;
}

t_stat id_dboot(int32 u, DEVICE *dptr)
{
    (void)u;
    (void)dptr;
    return SCPE_OK;
}

#include "id_idc.c"

static void test_idc_wds_returns_status_and_fills_sector_tail(void **state)
{
    FILE *file;
    UNIT unit = {0};
    uint8 sector[IDC_NUMBY];

    (void)state;

    file = tmpfile();
    assert_non_null(file);
    unit.fileref = file;

    memset(idcxb, 0, sizeof(idcxb));
    idcxb[0] = 001;
    idcxb[1] = 002;
    idc_bptr = 2;
    idc_db = 0377;

    assert_int_equal(idc_wds(&unit), SCPE_OK);
    assert_int_equal(idc_bptr, IDC_NUMBY);

    assert_int_equal(fflush(file), 0);
    assert_int_equal(fseek(file, 0, SEEK_SET), 0);
    assert_int_equal(fread(sector, 1, sizeof(sector), file), sizeof(sector));

    assert_int_equal(sector[0], 001);
    assert_int_equal(sector[1], 002);
    for (size_t i = 2; i < sizeof(sector); i++)
        assert_int_equal(sector[i], 0377);

    fclose(file);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_idc_wds_returns_status_and_fills_sector_tail),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
