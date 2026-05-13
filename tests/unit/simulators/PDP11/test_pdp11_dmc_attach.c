#include <stdarg.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "pdp11_defs.h"
#include "sim_tmxr.h"

int32_t int_req[IPL_HLVL];
uint32_t cpu_opt;
uint32_t cpu_type;
uint16_t *M;
jmp_buf save_env;
int32_t tmxr_poll;
int32_t clk_tps;
int32_t tmr_poll;

static DEVICE *test_find_dev_result;
static t_stat test_open_result;
static unsigned test_open_calls;
static TMXR *test_open_mux;
static char test_open_command[1024];
static unsigned test_activate_calls;
static UNIT *test_activate_unit;
static uint32_t test_activate_delay;
static unsigned test_printf_calls;

t_stat auto_config(const char *name, int32_t nctrl)
{
    (void)name;
    (void)nctrl;
    return SCPE_OK;
}

t_stat set_addr(UNIT *uptr, int32_t val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;
    return SCPE_OK;
}

t_stat show_addr(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;
    return SCPE_OK;
}

t_stat set_vec(UNIT *uptr, int32_t val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;
    return SCPE_OK;
}

t_stat show_vec(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;
    return SCPE_OK;
}

void cpu_set_boot(int32_t pc)
{
    (void)pc;
}

int32_t Map_ReadB(uint32_t ba, int32_t bc, uint8_t *buf)
{
    (void)ba;
    (void)bc;
    (void)buf;
    return 0;
}

int32_t Map_WriteB(uint32_t ba, int32_t bc, const uint8_t *buf)
{
    (void)ba;
    (void)bc;
    (void)buf;
    return 0;
}

DEVICE *test_find_dev_from_unit(UNIT *uptr)
{
    (void)uptr;
    return test_find_dev_result;
}

t_stat test_tmxr_open_master(TMXR *mp, const char *cptr)
{
    test_open_calls++;
    test_open_mux = mp;
    snprintf(test_open_command, sizeof(test_open_command), "%s", cptr);
    return test_open_result;
}

t_stat test_tmxr_activate_after(UNIT *uptr, uint32_t usecs_walltime)
{
    test_activate_calls++;
    test_activate_unit = uptr;
    test_activate_delay = usecs_walltime;
    return SCPE_OK;
}

int test_sim_printf(const char *fmt, ...)
{
    va_list args;

    test_printf_calls++;
    va_start(args, fmt);
    va_end(args);
    return 0;
}

#define find_dev_from_unit test_find_dev_from_unit
#define tmxr_open_master test_tmxr_open_master
#define tmxr_activate_after test_tmxr_activate_after
#define sim_printf test_sim_printf

#ifndef PDP11_DMC_SOURCE
#define PDP11_DMC_SOURCE "pdp11_dmc.c"
#endif

#include PDP11_DMC_SOURCE

static void reset_dmc_attach_state(void)
{
    free(dmc_units[0].filename);
    memset(dmc_peer, 0, sizeof(dmc_peer));
    memset(dmc_port, 0, sizeof(dmc_port));

    dmc_units[0] = dmc_unit_template;
    dmc_units[1] = dmc_poll_unit_template;
    dmc_units[0].flags = UNIT_ATTABLE;
    dmc_units[0].filename = NULL;
    dmc_desc.dptr = NULL;

    cpu_opt = BUS_U;
    test_find_dev_result = &dmc_dev;
    test_open_result = SCPE_OK;
    test_open_calls = 0;
    test_open_mux = NULL;
    test_open_command[0] = '\0';
    test_activate_calls = 0;
    test_activate_unit = NULL;
    test_activate_delay = 0;
    test_printf_calls = 0;
}

static void cleanup_dmc_attach_state(void)
{
    free(dmc_units[0].filename);
    dmc_units[0].filename = NULL;
}

static void test_attach_sync_commits_state_after_open_success(void **state)
{
    t_stat status;

    (void)state;
    reset_dmc_attach_state();

    status = dmc_attach(&dmc_units[0], "SYNC=sync0:integral:56000");

    assert_int_equal(status, SCPE_OK);
    assert_int_equal(test_open_calls, 1);
    assert_ptr_equal(test_open_mux, &dmc_desc);
    assert_string_equal(test_open_command, "Line=0,SYNC=sync0:integral:56000");
    assert_string_equal(dmc_port[0], "SYNC=sync0:integral:56000");
    assert_non_null(dmc_units[0].filename);
    assert_string_equal(dmc_units[0].filename, "SYNC=sync0:integral:56000");
    assert_true((dmc_units[0].flags & UNIT_ATT) != 0);
    assert_ptr_equal(dmc_desc.dptr, &dmc_dev);
    assert_int_equal(test_activate_calls, 1);
    assert_ptr_equal(test_activate_unit, &dmc_units[dmc_dev.numunits - 2]);
    assert_int_equal(test_activate_delay, DMC_CONNECT_POLL * 1000000);

    cleanup_dmc_attach_state();
}

static void test_attach_connect_uses_peer_in_open_command(void **state)
{
    t_stat status;

    (void)state;
    reset_dmc_attach_state();
    snprintf(dmc_peer[0], sizeof(dmc_peer[0]), "%s", "remote.example:2222");

    status = dmc_attach(&dmc_units[0], "localhost:1111");

    assert_int_equal(status, SCPE_OK);
    assert_int_equal(test_open_calls, 1);
    assert_string_equal(test_open_command,
                        "Line=0,Connect=remote.example:2222,localhost:1111");
    assert_string_equal(dmc_port[0], "localhost:1111");
    assert_string_equal(dmc_units[0].filename, "localhost:1111");
    assert_true((dmc_units[0].flags & UNIT_ATT) != 0);

    cleanup_dmc_attach_state();
}

static void test_attach_without_peer_does_not_call_open(void **state)
{
    t_stat status;

    (void)state;
    reset_dmc_attach_state();

    status = dmc_attach(&dmc_units[0], "localhost:1111");

    assert_int_equal(status, SCPE_ARG);
    assert_int_equal(test_open_calls, 0);
    assert_int_equal(test_activate_calls, 0);
    assert_int_equal(test_printf_calls, 1);
    assert_string_equal(dmc_port[0], "");
    assert_null(dmc_units[0].filename);
    assert_false((dmc_units[0].flags & UNIT_ATT) != 0);

    cleanup_dmc_attach_state();
}

static void test_attach_open_failure_leaves_previous_state(void **state)
{
    t_stat status;

    (void)state;
    reset_dmc_attach_state();
    snprintf(dmc_peer[0], sizeof(dmc_peer[0]), "%s", "remote.example:2222");
    snprintf(dmc_port[0], sizeof(dmc_port[0]), "%s", "old-listener:1000");
    dmc_units[0].filename = strdup("old-listener:1000");
    assert_non_null(dmc_units[0].filename);
    test_open_result = SCPE_OPENERR;

    status = dmc_attach(&dmc_units[0], "localhost:1111");

    assert_int_equal(status, SCPE_OPENERR);
    assert_int_equal(test_open_calls, 1);
    assert_string_equal(test_open_command,
                        "Line=0,Connect=remote.example:2222,localhost:1111");
    assert_int_equal(test_activate_calls, 0);
    assert_string_equal(dmc_port[0], "old-listener:1000");
    assert_string_equal(dmc_units[0].filename, "old-listener:1000");
    assert_false((dmc_units[0].flags & UNIT_ATT) != 0);

    cleanup_dmc_attach_state();
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_attach_sync_commits_state_after_open_success),
        cmocka_unit_test(test_attach_connect_uses_peer_in_open_command),
        cmocka_unit_test(test_attach_without_peer_does_not_call_open),
        cmocka_unit_test(test_attach_open_failure_leaves_previous_state),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
