#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "sim_defs.h"
#include "nd100_defs.h"

#define ND100_SHT_AD 0000600
#define ND100_SHT_RIGHT_32 0000040

extern int ins_mis(int IR, int off);
extern int ins_skip_ext(int IR);
extern int ins_sht(int IR, int off);
extern REG cpu_reg[];
extern uint16_t RBLK[16][8];

uint16_t PM[65536];
uint16_t M[65536];
REG *sim_PC = &cpu_reg[0];
DEVICE *sim_devices[] = {&cpu_dev, NULL};
char sim_name[] = "ND100";
const char *sim_stop_messages[SCPE_BASE];
int32_t sim_emax = 1;

static int last_prdmem_addr;
static int last_prdmem_how;
static uint16_t prdmem_result;
static int last_pwrmem_addr;
static int last_pwrmem_val;
static int last_pwrmem_how;

static void reset_cpu_registers(void)
{
    memset(R, 0, sizeof(R));
    memset(RBLK, 0, sizeof(RBLK));
    regSTH = 0;
    curlvl = 0;
    last_prdmem_addr = 0;
    last_prdmem_how = 0;
    prdmem_result = 0;
    last_pwrmem_addr = 0;
    last_pwrmem_val = 0;
    last_pwrmem_how = 0;
}

static void test_sht_ad_right_32_uses_full_count(void **state)
{
    (void)state;

    reset_cpu_registers();
    regA = 0x8000;
    regD = 0x0000;

    assert_int_equal(ins_sht(ND_SHT | ND100_SHT_AD | ND100_SHT_RIGHT_32, 0),
                     SCPE_OK);

    assert_int_equal(regA, 0xffff);
    assert_int_equal(regD, 0xffff);
}

static void test_sht_ad_left_1_uses_full_ad_bits(void **state)
{
    (void)state;

    reset_cpu_registers();
    regA = 0x8000;
    regD = 0x0001;

    assert_int_equal(ins_sht(ND_SHT | ND100_SHT_AD | 1, 0), SCPE_OK);

    assert_int_equal(regA, 0x0000);
    assert_int_equal(regD, 0x0002);
    assert_int_equal(regSTL & STS_M, STS_M);
}

static void test_skip_rdiv_uses_signed_ad_dividend(void **state)
{
    (void)state;

    reset_cpu_registers();
    regA = 0xffff;
    regD = 0xfffe;
    regT = 0xfffe;

    assert_int_equal(ins_skip_ext(ND_SKP_RDIV | (rnT << 3)), SCPE_OK);

    assert_int_equal(regA, 1);
    assert_int_equal(regD, 0);
}

static void test_mis_exam_uses_full_ad_address_bits(void **state)
{
    (void)state;

    reset_cpu_registers();
    regA = 0x8001;
    regD = 0x2345;
    prdmem_result = 0xabcd;

    assert_int_equal(ins_mis(ND_MIS_EXAM, 0), SCPE_OK);

    assert_int_equal((uint32_t)last_prdmem_addr, 0x80012345u);
    assert_int_equal(last_prdmem_how, M_FETCH);
    assert_int_equal(regT, prdmem_result);
}

static void test_mis_depo_uses_full_ad_address_bits(void **state)
{
    (void)state;

    reset_cpu_registers();
    regA = 0x8001;
    regD = 0x2345;
    regT = 0xcdef;

    assert_int_equal(ins_mis(ND_MIS_DEPO, 0), SCPE_OK);

    assert_int_equal((uint32_t)last_pwrmem_addr, 0x80012345u);
    assert_int_equal(last_pwrmem_val, regT);
    assert_int_equal(last_pwrmem_how, M_FETCH);
}

uint16_t prdmem(int addr, int how)
{
    last_prdmem_addr = addr;
    last_prdmem_how = how;
    return prdmem_result;
}

void pwrmem(int addr, int val, int how)
{
    last_pwrmem_addr = addr;
    last_pwrmem_val = val;
    last_pwrmem_how = how;
}

uint16_t rdmem(int addr, int how)
{
    (void)addr;
    (void)how;
    return 0;
}

void wrmem(int addr, int val, int how)
{
    (void)addr;
    (void)val;
    (void)how;
}

uint8_t rdbyte(int vaddr, int lr, int how)
{
    (void)vaddr;
    (void)lr;
    (void)how;
    return 0;
}

void wrbyte(int vaddr, int val, int lr, int how)
{
    (void)vaddr;
    (void)val;
    (void)lr;
    (void)how;
}

void mm_privcheck(void) {}

void mm_wrpcr(void) {}

int mm_tra(int reg)
{
    (void)reg;
    return SCPE_OK;
}

int iox_clk(int dev)
{
    (void)dev;
    return SCPE_OK;
}

int iox_floppy(int dev)
{
    (void)dev;
    return SCPE_OK;
}

int iox_tty(int dev)
{
    (void)dev;
    return SCPE_OK;
}

t_stat sim_load(FILE *ptr, const char *cptr, const char *fnam, int flag)
{
    (void)ptr;
    (void)cptr;
    (void)fnam;
    (void)flag;
    return SCPE_OK;
}

t_stat fprint_sym(FILE *ofile, t_addr addr, t_value *val, UNIT *uptr, int32_t sw)
{
    (void)ofile;
    (void)addr;
    (void)val;
    (void)uptr;
    (void)sw;
    return SCPE_OK;
}

t_stat parse_sym(const char *cptr, t_addr addr, UNIT *uptr, t_value *val,
                 int32_t sw)
{
    (void)cptr;
    (void)addr;
    (void)uptr;
    (void)val;
    (void)sw;
    return SCPE_OK;
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sht_ad_right_32_uses_full_count),
        cmocka_unit_test(test_sht_ad_left_1_uses_full_ad_bits),
        cmocka_unit_test(test_skip_rdiv_uses_signed_ad_dividend),
        cmocka_unit_test(test_mis_exam_uses_full_ad_address_bits),
        cmocka_unit_test(test_mis_depo_uses_full_ad_address_bits),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
