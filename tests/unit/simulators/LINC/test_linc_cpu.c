#include <setjmp.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "linc_defs.h"
#include "scp.h"

#define REG_P 0
#define REG_PAUSED 11
#define REG_XL 18

#define LINC_HLT 00000
#define LINC_OPR_I_6 00526

uint16_t M[MEMSIZE];
DEVICE crt_dev;
REG *sim_PC = &cpu_reg[REG_P];
DEVICE *sim_devices[] = {&cpu_dev, &crt_dev, NULL};
char sim_name[] = "LINC";
const char *sim_stop_messages[SCPE_BASE];
int32_t sim_emax = 1;

static int restart_event_seen;
static UNIT restart_unit;
static int unexpected_pause_event_seen;
static UNIT unexpected_pause_unit;

static uint16_t *reg_u16(size_t index)
{
    return (uint16_t *)cpu_reg[index].loc;
}

static int *reg_int(size_t index)
{
    return (int *)cpu_reg[index].loc;
}

static uint16_t *reg_array(size_t index)
{
    return (uint16_t *)cpu_reg[index].loc;
}

t_stat build_dev_tab(void)
{
    return SCPE_OK;
}

uint16_t kbd_key(uint16_t wait)
{
    (void)wait;
    return 0;
}

int kbd_struck(void)
{
    return 0;
}

void tape_op(void) {}

void crt_toggle_fullscreen(void) {}

void dpy_dis(uint16_t h, uint16_t x, uint16_t y)
{
    (void)h;
    (void)x;
    (void)y;
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

static t_stat restart_opr_pause(UNIT *uptr)
{
    uint16_t *xl;

    (void)uptr;

    restart_event_seen = 1;
    assert_int_equal(*reg_int(REG_PAUSED), 1);

    xl = reg_array(REG_XL);
    xl[6] = 1;
    return SCPE_OK;
}

static t_stat unexpected_opr_pause(UNIT *uptr)
{
    (void)uptr;

    unexpected_pause_event_seen = 1;
    assert_int_equal(*reg_int(REG_PAUSED), 0);
    return SCPE_OK;
}

static void reset_cpu_fixture(void)
{
    uint16_t *xl;

    memset(M, 0, sizeof(M));
    *reg_u16(REG_P) = 0;
    *reg_int(REG_PAUSED) = 0;
    xl = reg_array(REG_XL);
    memset(xl, 0, 12 * sizeof(*xl));
    restart_event_seen = 0;
    unexpected_pause_event_seen = 0;
    sim_interval = 0;
    sim_step = 0;
    sim_cancel(&restart_unit);
    sim_cancel(&unexpected_pause_unit);
}

static void test_opr_i_pauses_until_external_level(void **state)
{
    t_stat stat;

    (void)state;

    reset_cpu_fixture();
    restart_unit.action = restart_opr_pause;
    M[0] = LINC_OPR_I_6;
    M[1] = LINC_HLT;
    sim_activate(&restart_unit, 1);

    stat = sim_instr();

    assert_int_equal(stat, STOP_HALT);
    assert_true(restart_event_seen);
    assert_int_equal(*reg_u16(REG_P), 2);
}

static void test_opr_i_does_not_pause_if_external_level_set(void **state)
{
    t_stat stat;
    uint16_t *xl;

    (void)state;

    reset_cpu_fixture();
    unexpected_pause_unit.action = unexpected_opr_pause;
    xl = reg_array(REG_XL);
    xl[6] = 1;
    M[0] = LINC_OPR_I_6;
    M[1] = LINC_HLT;
    sim_activate(&unexpected_pause_unit, 5);

    stat = sim_instr();

    assert_int_equal(stat, STOP_HALT);
    assert_false(unexpected_pause_event_seen);
    assert_int_equal(*reg_u16(REG_P), 2);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_opr_i_pauses_until_external_level),
        cmocka_unit_test(test_opr_i_does_not_pause_if_external_level_set),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
