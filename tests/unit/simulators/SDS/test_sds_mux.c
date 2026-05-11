#include <stdint.h>

#include "test_cmocka.h"

#include "sds_defs.h"

extern t_stat mux(uint32_t fnc, uint32_t inst, uint32_t *dat);

uint32_t alert;
uint32_t int_req;
int32_t stop_invins;
int32_t stop_invdev;
int32_t stop_inviop;
UNIT cpu_unit;

/*
 * A standard SDS 940 SKS XBE instruction for an inactive mux line should be a
 * valid skip test, not an internal simulator error.
 */
static void test_mux_sks_xbe_on_inactive_line_returns_ok(void **state)
{
    uint32_t skip = 0;

    (void)state;

    cpu_unit.flags = 0;

    assert_int_equal(mux(IO_SKS, 024077000, &skip), SCPE_OK);
    assert_int_equal(skip, 1);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_mux_sks_xbe_on_inactive_line_returns_ok),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
