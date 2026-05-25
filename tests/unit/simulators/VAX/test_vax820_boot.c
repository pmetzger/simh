#include <setjmp.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "vax_cpu.h"
#include "vax_defs.h"
#include "vax_kdb50_internal.h"

uint32_t R[16];
uint32_t PSL;
uint32_t SISR;
uint32_t fault_PC;
uint32_t p1;
uint32_t p2;
uint32_t trpirq;
uint32_t mchk_ref;
uint32_t *M;
int32_t hlt_pin;
int32_t crd_err;
int32_t tmr_int;
int32_t fl_int;
int32_t tti_int;
int32_t tto_int;
int32_t cur_cpu;
int32_t cpu_msk = 1;
int32_t in_ie;
int32_t mapen;
uint32_t mchk_va;
TLBENT stlb[VA_TBSIZE];
TLBENT ptlb[VA_TBSIZE];
jmp_buf save_env;
UNIT cpu_unit;
DEVICE cpu_dev;

static UNIT kdb50_units[2];
static DIB kdb50_dib = {
    KDB50_DEFAULT_NEXUS, 0, NULL, NULL, 0, 0,
};
static int32_t kdb50_vector_level;
static int32_t kdb50_vector_calls;
static int32_t kdb50_vector = 0x0114;

DEVICE kdb50_dev = {
    .name = "KDB",
    .units = kdb50_units,
    .numunits = 2,
    .aradix = 10,
    .awidth = 31,
    .aincr = 1,
    .dradix = 8,
    .dwidth = 8,
    .ctxt = &kdb50_dib,
};
DEVICE *sim_devices[] = {&kdb50_dev, NULL};

/*
 * Include the implementation directly so the test can exercise the boot
 * parser without exporting a production-only test hook.
 */

void uba_eval_int(void) {}

int32_t uba_get_ubvector(int32_t lvl)
{
    (void)lvl;

    return 0;
}

int32_t kdb50_get_vector(int32_t lvl)
{
    kdb50_vector_level = lvl;
    kdb50_vector_calls++;

    return kdb50_vector;
}

int32_t iccs_rd(void)
{
    return 0;
}
int32_t nicr_rd(void)
{
    return 0;
}
int32_t icr_rd(void)
{
    return 0;
}
int32_t todr_rd(void)
{
    return 0;
}
int32_t rxcs_rd(void)
{
    return 0;
}
int32_t rxdb_rd(void)
{
    return 0;
}
int32_t txcs_rd(void)
{
    return 0;
}
int32_t rxcd_rd(void)
{
    return 0;
}
int32_t ReadIO(uint32_t pa, int32_t lnt)
{
    (void)pa;
    (void)lnt;

    return 0;
}
int32_t wtc_rd_pa(int32_t pa)
{
    (void)pa;

    return 0;
}
int32_t pcsr_rd(int32_t pa)
{
    (void)pa;

    return 0;
}
int32_t fl_rd(int32_t pa)
{
    (void)pa;

    return 0;
}

void iccs_wr(int32_t val)
{
    (void)val;
}
void nicr_wr(int32_t val)
{
    (void)val;
}
void todr_wr(int32_t val)
{
    (void)val;
}
void rxcs_wr(int32_t val)
{
    (void)val;
}
void txcs_wr(int32_t val)
{
    (void)val;
}
void txdb_wr(int32_t val)
{
    (void)val;
}
void rxcd_wr(int32_t val)
{
    (void)val;
}
void WriteIO(uint32_t pa, int32_t val, int32_t lnt)
{
    (void)pa;
    (void)val;
    (void)lnt;
}
void wtc_wr_pa(int32_t pa, int32_t val, int32_t lnt)
{
    (void)pa;
    (void)val;
    (void)lnt;
}
void pcsr_wr(int32_t pa, int32_t val, int32_t lnt)
{
    (void)pa;
    (void)val;
    (void)lnt;
}
void fl_wr(int32_t pa, int32_t val, int32_t lnt)
{
    (void)pa;
    (void)val;
    (void)lnt;
}

TLBENT fill(uint32_t va, int32_t lnt, int32_t acc, int32_t *stat)
{
    TLBENT ent = {0};

    (void)va;
    (void)lnt;
    (void)acc;
    if (stat != NULL)
        *stat = 0;
    return ent;
}

int32_t intexc(int32_t vec, int32_t cc, int32_t ipl, int ei)
{
    (void)vec;
    (void)cc;
    (void)ipl;
    (void)ei;

    return 0;
}

t_stat cpu_load_bootcode(const char *filename, const uchar_t *data, size_t size,
                         bool is_rom, t_addr origin)
{
    (void)filename;
    (void)data;
    (void)size;
    (void)is_rom;
    (void)origin;

    return SCPE_OK;
}

void init_ubus_tab(void) {}
t_stat build_ubus_tab(DEVICE *dptr, DIB *dibp)
{
    (void)dptr;
    (void)dibp;

    return SCPE_OK;
}

#include "vax820_bi.c"

static void clear_boot_registers(void)
{
    memset(R, 0xa5, sizeof(R));
}

static void clear_interrupt_state(void)
{
    memset(nexus_req, 0, sizeof(nexus_req));
    kdb50_vector_level = -1;
    kdb50_vector_calls = 0;
    kdb50_vector = 0x0114;
}

static void test_kdb_nexus_interrupt_uses_kdb50_vector(void **state)
{
    int32_t vector;

    (void)state;

    clear_interrupt_state();
    nexus_req[IPL_KDB50] = 1u << TR_KDB50;

    vector = get_vector(IPL_HMIN + IPL_KDB50);

    assert_int_equal(vector, 0x0114);
    assert_int_equal(kdb50_vector_calls, 1);
    assert_int_equal(kdb50_vector_level, IPL_KDB50);
    assert_int_equal(nexus_req[IPL_KDB50] & (1u << TR_KDB50), 0);
}

static void test_kdb_boot_sets_vmb_registers(void **state)
{
    (void)state;

    clear_boot_registers();

    assert_int_equal(vax820_boot_parse(0, "KDB1/R5:2"), SCPE_OK);
    assert_int_equal(R[0], BOOT_KDB);
    assert_int_equal(R[1], KDB50_DEFAULT_NEXUS);
    assert_int_equal(R[2], REGBASE + (KDB50_DEFAULT_NEXUS << REG_V_NEXUS));
    assert_int_equal(R[3], 1);
    assert_int_equal(R[4], 0);
    assert_int_equal(R[5], 2);
}

static void test_kdb_boot_accepts_equals_r5_separator(void **state)
{
    (void)state;

    clear_boot_registers();

    assert_int_equal(vax820_boot_parse(0, "KDB0/r5=8"), SCPE_OK);
    assert_int_equal(R[0], BOOT_KDB);
    assert_int_equal(R[3], 0);
    assert_int_equal(R[5], 8);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_kdb_nexus_interrupt_uses_kdb50_vector),
        cmocka_unit_test(test_kdb_boot_sets_vmb_registers),
        cmocka_unit_test(test_kdb_boot_accepts_equals_r5_separator),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
