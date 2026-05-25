/* test_sim_uuid.c: tests for host UUID helpers */

#include <errno.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sim_uuid.h"
#include "sim_uuid_internal.h"
#include "test_cmocka.h"

static int uuid_is_nil(const uint8_t uuid[16])
{
    uint8_t zero[16] = {0};

    return memcmp(uuid, zero, sizeof(zero)) == 0;
}

static int uuid_has_rfc4122_variant(const uint8_t uuid[16])
{
    return (uuid[8] & 0xc0) == 0x80;
}

/* Verify structured UUID fields are encoded into canonical byte order. */
static void test_sim_uuid_encode_fields_uses_canonical_order(void **state)
{
    const uint8_t clock_seq_and_node[8] = {0x80, 0x9a, 0xbc, 0xde,
                                           0xf0, 0x12, 0x34, 0x56};
    const uint8_t expected[16] = {0x12, 0x34, 0x56, 0x78, 0xab, 0xcd,
                                  0x4e, 0xf0, 0x80, 0x9a, 0xbc, 0xde,
                                  0xf0, 0x12, 0x34, 0x56};
    uint8_t uuid[16];

    (void)state;

    memset(uuid, 0, sizeof(uuid));
    sim_uuid_encode_fields(uuid, 0x12345678u, 0xabcdu, 0x4ef0u,
                           clock_seq_and_node);
    assert_memory_equal(uuid, expected, sizeof(expected));
}

/* Verify invalid output storage is rejected. */
static void test_sim_uuid_generate_rejects_null(void **state)
{
    (void)state;

    errno = 0;
    assert_int_equal(sim_uuid_generate(NULL), -1);
    assert_int_equal(errno, EINVAL);
}

/* Verify UUID generation returns a populated RFC 4122-style UUID. */
static void test_sim_uuid_generate_returns_uuid(void **state)
{
    uint8_t uuid[16];

    (void)state;

    memset(uuid, 0, sizeof(uuid));
    assert_int_equal(sim_uuid_generate(uuid), 0);
    assert_false(uuid_is_nil(uuid));
    assert_true(uuid_has_rfc4122_variant(uuid));
}

/* Verify repeated UUID generation does not return a constant value. */
static void test_sim_uuid_generate_changes_between_calls(void **state)
{
    uint8_t first[16];
    uint8_t next[16];
    int saw_difference = 0;
    int i;

    (void)state;

    assert_int_equal(sim_uuid_generate(first), 0);
    for (i = 0; i < 8; i++) {
        assert_int_equal(sim_uuid_generate(next), 0);
        if (memcmp(first, next, sizeof(first)) != 0)
            saw_difference = 1;
    }

    assert_true(saw_difference);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sim_uuid_encode_fields_uses_canonical_order),
        cmocka_unit_test(test_sim_uuid_generate_rejects_null),
        cmocka_unit_test(test_sim_uuid_generate_returns_uuid),
        cmocka_unit_test(test_sim_uuid_generate_changes_between_calls),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
