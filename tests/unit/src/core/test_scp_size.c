#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "scp_size.h"

/* Verify raw byte widths map into the expected SCP storage buckets. */
static void test_storage_size_buckets(void **state)
{
    (void)state;

    assert_int_equal(scp_storage_size_for_width_bytes(0), sizeof(int8_t));
    assert_int_equal(scp_storage_size_for_width_bytes(1), sizeof(int8_t));
    assert_int_equal(scp_storage_size_for_width_bytes(2), sizeof(int16_t));
    assert_int_equal(scp_storage_size_for_width_bytes(3), sizeof(int32_t));
    assert_int_equal(scp_storage_size_for_width_bytes(4), sizeof(int32_t));
#if defined(USE_INT64)
    assert_int_equal(scp_storage_size_for_width_bytes(5), sizeof(int64_t));
    assert_int_equal(scp_storage_size_for_width_bytes(8), sizeof(int64_t));
    assert_int_equal(scp_storage_size_for_width_bytes(9), 0);
#else
    assert_int_equal(scp_storage_size_for_width_bytes(5), 0);
#endif
}

/* Verify device data widths map to the right storage size. */
static void test_device_width_maps_to_storage_size(void **state)
{
    DEVICE dptr;

    (void)state;

    memset(&dptr, 0, sizeof(dptr));

    dptr.dwidth = 8;
    assert_int_equal(scp_device_data_size_bytes(&dptr), sizeof(int8_t));

    dptr.dwidth = 16;
    assert_int_equal(scp_device_data_size_bytes(&dptr), sizeof(int16_t));

    dptr.dwidth = 24;
    assert_int_equal(scp_device_data_size_bytes(&dptr), sizeof(int32_t));

    dptr.dwidth = 32;
    assert_int_equal(scp_device_data_size_bytes(&dptr), sizeof(int32_t));
}

/* Verify register width plus offset maps to the right storage size. */
static void test_register_width_and_offset_map_to_storage_size(void **state)
{
    REG rptr;

    (void)state;

    memset(&rptr, 0, sizeof(rptr));

    rptr.width = 8;
    rptr.offset = 0;
    assert_int_equal(scp_register_data_size_bytes(&rptr), sizeof(int8_t));

    rptr.width = 12;
    rptr.offset = 4;
    assert_int_equal(scp_register_data_size_bytes(&rptr), sizeof(int16_t));

    rptr.width = 24;
    rptr.offset = 0;
    assert_int_equal(scp_register_data_size_bytes(&rptr), sizeof(int32_t));

#if defined(USE_INT64)
    rptr.width = 36;
    rptr.offset = 0;
    assert_int_equal(scp_register_data_size_bytes(&rptr), sizeof(int64_t));

    rptr.width = 65;
    rptr.offset = 0;
    assert_int_equal(scp_register_data_size_bytes(&rptr), 0);
#else
    rptr.width = 36;
    rptr.offset = 0;
    assert_int_equal(scp_register_data_size_bytes(&rptr), 0);
#endif
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_storage_size_buckets),
        cmocka_unit_test(test_device_width_maps_to_storage_size),
        cmocka_unit_test(test_register_width_and_offset_map_to_storage_size),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
