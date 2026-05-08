#include <stddef.h>
#include <string.h>

#include "test_cmocka.h"

#include "vax750_mem_internal.h"

extern uint32 rom[];

void rom_wr_B(int32 pa, int32 val);

UNIT cpu_unit;

t_stat show_nexus(FILE *st, UNIT *uptr, int32 val, const void *desc)
{
    /* Stubbed modifier callback for the MCTL modifier table. */
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;

    return SCPE_OK;
}

t_stat cpu_load_bootcode(const char *filename,
                         const unsigned char *builtin_code, size_t size,
                         t_bool load_rom, t_addr offset)
{
    /* Stubbed boot-code loader for uncalled boot paths. */
    (void)filename;
    (void)builtin_code;
    (void)size;
    (void)load_rom;
    (void)offset;

    return SCPE_OK;
}

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

/* Verify ROM byte writes preserve neighboring bytes and mask the source. */
static void test_rom_write_byte_updates_each_byte(void **state)
{
    static const struct {
        uint32 addr;
        uint32 val;
        uint32 expected;
    } cases[] = {
        {0, 0x1a5, 0x123456a5},
        {1, 0x180, 0x12348078},
        {2, 0x17e, 0x127e5678},
        {3, 0x1ff, 0xff345678},
    };
    size_t i;

    (void)state;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        memset(rom, 0, ROMSIZE);
        rom[0] = 0x12345678;
        rom_wr_B((int32)(ROMBASE + cases[i].addr), (int32)cases[i].val);
        assert_int_equal(rom[0], cases[i].expected);
    }
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_mcsr2_reset_value_covers_supported_memory_sizes),
        cmocka_unit_test(test_rom_write_byte_updates_each_byte),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
