#include "test_cmocka.h"

#include "sel32_defs.h"

struct ipcom *IPC;
uint32 M[4];
uint32 MAPC[4];

/* Verify byte writes preserve neighboring bytes and handle the high byte. */
static void test_write_memory_byte_updates_each_byte(void **state)
{
    static const struct {
        uint32 addr;
        uint32 value;
        uint32 expected;
    } cases[] = {
        {0, 0xff, 0xff000000},
        {1, 0x7e, 0xff7e0000},
        {2, 0x80, 0xff7e8000},
        {3, 0x01, 0xff7e8001},
    };

    (void)state;

    M[0] = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        WMB(cases[i].addr, cases[i].value);
        assert_int_equal(M[0], cases[i].expected);
        assert_int_equal(RMB(cases[i].addr), cases[i].value);
    }
}

/* Verify halfword writes handle high-bit values in both halfwords. */
static void test_write_memory_halfword_updates_each_half(void **state)
{
    (void)state;

    M[0] = 0x12345678;
    WMH(0, 0x8001);
    assert_int_equal(M[0], 0x80015678);
    assert_int_equal(RMH(0), 0x8001);
    assert_int_equal(RMH(2), 0x5678);

    WMH(2, 0xffff);
    assert_int_equal(M[0], 0x8001ffff);
    assert_int_equal(RMH(0), 0x8001);
    assert_int_equal(RMH(2), 0xffff);
}

/* Verify MAPC halfword writes use the same unsigned bit construction. */
static void test_write_map_cache_halfword_updates_each_half(void **state)
{
    (void)state;

    MAPC[0] = 0x12345678;
    WMR(0, 0xffff);
    assert_int_equal(MAPC[0], 0xffff5678);
    assert_int_equal(RMR(0), 0xffff);
    assert_int_equal(RMR(2), 0x5678);

    WMR(2, 0x8001);
    assert_int_equal(MAPC[0], 0xffff8001);
    assert_int_equal(RMR(0), 0xffff);
    assert_int_equal(RMR(2), 0x8001);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_write_memory_byte_updates_each_byte),
        cmocka_unit_test(test_write_memory_halfword_updates_each_half),
        cmocka_unit_test(test_write_map_cache_halfword_updates_each_half),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
