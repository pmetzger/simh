#include <stddef.h>

#include "test_cmocka.h"

#include "vax750_mem_internal.h"

/* Verify MCSR2 memory configuration bits for each supported VAX-750 size. */
static void test_mcsr2_reset_value_covers_supported_memory_sizes(void **state)
{
    static const struct {
        uint32 memsize;
        uint32 mcsr2;
    } cases[] = {
        {1u << 20, 0x000100ff},
        {1u << 21, 0x0001ffff},
        {1u << 22, 0x00010055},
        {1u << 23, 0x00015555},
        {(1u << 23) + (1u << 22), 0x02010015},
        {(1u << 23) + (6u << 20), 0x02010295},
        {(1u << 23) + (7u << 20), 0x02010a95},
    };
    size_t i;

    (void)state;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        assert_int_equal(vax750_mcsr2_reset_value(cases[i].memsize),
                         cases[i].mcsr2);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_mcsr2_reset_value_covers_supported_memory_sizes),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
