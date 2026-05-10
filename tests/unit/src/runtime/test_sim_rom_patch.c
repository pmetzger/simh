#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "sim_defs.h"
#include "sim_rom_patch.h"

struct patch_memory {
    uint8_t bytes[16];
    size_t write_count;
};

static t_stat read_byte(void *context, t_addr address, uint8_t *value)
{
    struct patch_memory *memory = context;

    if (address >= sizeof(memory->bytes))
        return SCPE_NXM;
    *value = memory->bytes[address];
    return SCPE_OK;
}

static t_stat write_byte(void *context, t_addr address, uint8_t value)
{
    struct patch_memory *memory = context;

    if (address >= sizeof(memory->bytes))
        return SCPE_NXM;
    memory->bytes[address] = value;
    memory->write_count++;
    return SCPE_OK;
}

static const uint8_t original[] = {0x10, 0x20, 0x30, 0x40};
static const uint8_t patched[] = {0xaa, 0xbb, 0xcc, 0xdd};

static const sim_rom_patch test_patch = {
    "TEST-PATCH", "test patch", 4, original, patched, sizeof(original),
};

static void test_apply_writes_replacement_when_original_matches(void **state)
{
    struct patch_memory memory = {0};
    (void)state;

    memcpy(&memory.bytes[4], original, sizeof(original));

    assert_int_equal(
        sim_rom_patch_apply(&test_patch, read_byte, write_byte, &memory),
        SCPE_OK);

    assert_memory_equal(&memory.bytes[4], patched, sizeof(patched));
    assert_int_equal(memory.write_count, sizeof(patched));
}

static void test_apply_is_idempotent_when_already_patched(void **state)
{
    struct patch_memory memory = {0};
    (void)state;

    memcpy(&memory.bytes[4], patched, sizeof(patched));

    assert_int_equal(
        sim_rom_patch_apply(&test_patch, read_byte, write_byte, &memory),
        SCPE_OK);

    assert_memory_equal(&memory.bytes[4], patched, sizeof(patched));
    assert_int_equal(memory.write_count, 0);
}

static void test_apply_rejects_unexpected_original_bytes(void **state)
{
    struct patch_memory memory = {0};
    (void)state;

    memcpy(&memory.bytes[4], original, sizeof(original));
    memory.bytes[5] = 0x99;

    assert_int_equal(
        sim_rom_patch_apply(&test_patch, read_byte, write_byte, &memory),
        SCPE_ARG);

    assert_memory_equal(&memory.bytes[4], ((uint8_t[]){0x10, 0x99, 0x30, 0x40}),
                        sizeof(original));
    assert_int_equal(memory.write_count, 0);
}

static void test_revert_restores_original_when_replacement_matches(void **state)
{
    struct patch_memory memory = {0};
    (void)state;

    memcpy(&memory.bytes[4], patched, sizeof(patched));

    assert_int_equal(
        sim_rom_patch_revert(&test_patch, read_byte, write_byte, &memory),
        SCPE_OK);

    assert_memory_equal(&memory.bytes[4], original, sizeof(original));
    assert_int_equal(memory.write_count, sizeof(original));
}

static void test_revert_is_idempotent_when_original_matches(void **state)
{
    struct patch_memory memory = {0};
    (void)state;

    memcpy(&memory.bytes[4], original, sizeof(original));

    assert_int_equal(
        sim_rom_patch_revert(&test_patch, read_byte, write_byte, &memory),
        SCPE_OK);

    assert_memory_equal(&memory.bytes[4], original, sizeof(original));
    assert_int_equal(memory.write_count, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_apply_writes_replacement_when_original_matches),
        cmocka_unit_test(test_apply_is_idempotent_when_already_patched),
        cmocka_unit_test(test_apply_rejects_unexpected_original_bytes),
        cmocka_unit_test(
            test_revert_restores_original_when_replacement_matches),
        cmocka_unit_test(test_revert_is_idempotent_when_original_matches),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
