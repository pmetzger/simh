#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"
#include "test_simh_personality.h"

#include "system_defs.h"

extern int model;
extern int mem_map;

t_stat sys_set_model(UNIT *uptr, int32_t val, const char *cptr, void *desc);

uint16_t PCX;

static t_stat cfg_stub(uint16_t val1, uint16_t val2, uint8_t val3)
{
    (void)val1;
    (void)val2;
    (void)val3;

    return SCPE_OK;
}

static t_stat clr_stub(void)
{
    return SCPE_OK;
}

uint8_t reg_dev(uint8_t (*routine)(bool, uint8_t, uint8_t), uint16_t base,
                uint16_t devnum, uint8_t dummy)
{
    (void)routine;
    (void)base;
    (void)devnum;
    (void)dummy;

    return 0;
}

uint8_t unreg_dev(uint16_t devnum)
{
    (void)devnum;

    return 0;
}

t_stat i3214_cfg(uint16_t base, uint16_t devnum, uint8_t dummy)
{
    return cfg_stub(base, devnum, dummy);
}

t_stat i8251_cfg(uint16_t base, uint16_t devnum, uint8_t dummy)
{
    return cfg_stub(base, devnum, dummy);
}

t_stat i8253_cfg(uint16_t base, uint16_t devnum, uint8_t dummy)
{
    return cfg_stub(base, devnum, dummy);
}

t_stat i8255_cfg(uint16_t base, uint16_t devnum, uint8_t dummy)
{
    return cfg_stub(base, devnum, dummy);
}

t_stat i8259_cfg(uint16_t base, uint16_t devnum, uint8_t dummy)
{
    return cfg_stub(base, devnum, dummy);
}

t_stat ioc_cont_cfg(uint16_t base, uint16_t devnum, uint8_t dummy)
{
    return cfg_stub(base, devnum, dummy);
}

t_stat ipc_cont_cfg(uint16_t base, uint16_t devnum, uint8_t dummy)
{
    return cfg_stub(base, devnum, dummy);
}

t_stat EPROM_cfg(uint16_t base, uint16_t size, uint8_t devnum)
{
    return cfg_stub(base, size, devnum);
}

t_stat RAM_cfg(uint16_t base, uint16_t size, uint8_t dummy)
{
    return cfg_stub(base, size, dummy);
}

t_stat isbc064_cfg(uint16_t base, uint16_t size, uint8_t dummy)
{
    return cfg_stub(base, size, dummy);
}

t_stat isbc464_cfg(uint16_t base, uint16_t size, uint8_t dummy)
{
    return cfg_stub(base, size, dummy);
}

t_stat isbc201_cfg(uint16_t base, uint16_t size, uint8_t dummy)
{
    return cfg_stub(base, size, dummy);
}

t_stat isbc202_cfg(uint16_t base, uint16_t size, uint8_t dummy)
{
    return cfg_stub(base, size, dummy);
}

t_stat isbc206_cfg(uint16_t base, uint16_t size, uint8_t dummy)
{
    return cfg_stub(base, size, dummy);
}

t_stat isbc208_cfg(uint16_t base, uint16_t size, uint8_t dummy)
{
    return cfg_stub(base, size, dummy);
}

t_stat i3214_clr(void)
{
    return clr_stub();
}

t_stat i8251_clr(void)
{
    return clr_stub();
}

t_stat i8253_clr(void)
{
    return clr_stub();
}

t_stat i8255_clr(void)
{
    return clr_stub();
}

t_stat i8259_clr(void)
{
    return clr_stub();
}

t_stat ioc_cont_clr(void)
{
    return clr_stub();
}

t_stat ipc_cont_clr(void)
{
    return clr_stub();
}

t_stat EPROM_clr(void)
{
    return clr_stub();
}

t_stat RAM_clr(void)
{
    return clr_stub();
}

t_stat isbc064_clr(void)
{
    return clr_stub();
}

t_stat isbc464_clr(void)
{
    return clr_stub();
}

t_stat isbc201_clr(void)
{
    return clr_stub();
}

t_stat isbc202_clr(void)
{
    return clr_stub();
}

t_stat isbc208_clr(void)
{
    return clr_stub();
}

void clr_dev(void) {}

static int setup_sys_model(void **state)
{
    (void)state;

    simh_test_reset_simulator_state();
    memset(sim_name, 'X', sizeof(sim_name));
    sim_name[sizeof(sim_name) - 1] = '\0';
    model = -1;
    mem_map = 0;

    return 0;
}

/*
 * An 11-character model name must leave the simulator name NUL-terminated.
 */
static void test_set_model_terminates_exact_width_model_name(void **state)
{
    (void)state;

    assert_int_equal(sys_set_model(NULL, 0, "SYS-80/10-0", NULL), SCPE_OK);

    assert_memory_equal(sim_name, "SYS-80/10-0", 11);
    assert_int_equal(sim_name[11], '\0');
    assert_string_equal(sim_name, "SYS-80/10-0");
    assert_int_equal(model, 14);
    assert_int_equal(mem_map, 4);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_set_model_terminates_exact_width_model_name,
                               setup_sys_model),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
