#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "pdp11_defs.h"

extern DEVICE tti_dev;
extern DEVICE tto_dev;
extern UNIT tti_unit;
extern int32_t int_req[IPL_HLVL];

t_stat tti_reset(DEVICE *dptr);
t_stat tto_reset(DEVICE *dptr);

int32_t int_req[IPL_HLVL];
uint32_t cpu_opt;
uint32_t cpu_type;
jmp_buf save_env;

static char auto_config_name[8];
static int32_t auto_config_nctrl;

t_stat auto_config(const char *name, int32_t nctrl)
{
    assert_non_null(name);

    snprintf(auto_config_name, sizeof(auto_config_name), "%s", name);
    auto_config_nctrl = nctrl;
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

t_stat show_vec(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;
    return SCPE_OK;
}

static void reset_auto_config_capture(void)
{
    auto_config_name[0] = '\0';
    auto_config_nctrl = -1;
}

static void test_tti_and_tto_are_disable_capable(void **state)
{
    (void)state;

    assert_true(tti_dev.flags & DEV_DISABLE);
    assert_true(tto_dev.flags & DEV_DISABLE);
}

static void test_tti_reset_autoconfigures_enabled_device(void **state)
{
    (void)state;
    reset_auto_config_capture();
    tti_dev.flags &= ~DEV_DIS;

    assert_int_equal(tti_reset(&tti_dev), SCPE_OK);

    assert_string_equal(auto_config_name, "TTI");
    assert_int_equal(auto_config_nctrl, 1);
    sim_cancel(&tti_unit);
}

static void test_tti_reset_removes_disabled_device_from_autoconfig(void **state)
{
    (void)state;
    reset_auto_config_capture();
    tti_dev.flags |= DEV_DIS;

    assert_int_equal(tti_reset(&tti_dev), SCPE_OK);

    assert_string_equal(auto_config_name, "TTI");
    assert_int_equal(auto_config_nctrl, 0);
    tti_dev.flags &= ~DEV_DIS;
    sim_cancel(&tti_unit);
}

static void test_tto_reset_autoconfigures_enabled_device(void **state)
{
    (void)state;
    reset_auto_config_capture();
    tto_dev.flags &= ~DEV_DIS;

    assert_int_equal(tto_reset(&tto_dev), SCPE_OK);

    assert_string_equal(auto_config_name, "TTO");
    assert_int_equal(auto_config_nctrl, 1);
}

static void test_tto_reset_removes_disabled_device_from_autoconfig(void **state)
{
    (void)state;
    reset_auto_config_capture();
    tto_dev.flags |= DEV_DIS;

    assert_int_equal(tto_reset(&tto_dev), SCPE_OK);

    assert_string_equal(auto_config_name, "TTO");
    assert_int_equal(auto_config_nctrl, 0);
    tto_dev.flags &= ~DEV_DIS;
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_tti_and_tto_are_disable_capable),
        cmocka_unit_test(test_tti_reset_autoconfigures_enabled_device),
        cmocka_unit_test(
            test_tti_reset_removes_disabled_device_from_autoconfig),
        cmocka_unit_test(test_tto_reset_autoconfigures_enabled_device),
        cmocka_unit_test(
            test_tto_reset_removes_disabled_device_from_autoconfig),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
