#include <stdio.h>

#include "test_cmocka.h"

#include "sim_defs.h"

#define ASSERT_FORMAT(expected, format, ...)                                   \
    do {                                                                       \
        char buffer[128];                                                      \
        int written = snprintf(buffer, sizeof(buffer), format, __VA_ARGS__);   \
                                                                               \
        assert_true(written >= 0);                                             \
        assert_true((size_t)written < sizeof(buffer));                         \
        assert_string_equal(buffer, expected);                                 \
    } while (0)

#if defined(USE_INT64) && defined(USE_ADDR64)
typedef int64_t test_addr_signed_t;
#define TEST_ADDR_MAX_DEC "18446744073709551615"
#define TEST_ADDR_MAX_OCT "1777777777777777777777"
#define TEST_ADDR_MAX_HEX "ffffffffffffffff"
#define TEST_ADDR_MAX_HEX_UPPER "FFFFFFFFFFFFFFFF"
#else
typedef int32_t test_addr_signed_t;
#define TEST_ADDR_MAX_DEC "4294967295"
#define TEST_ADDR_MAX_OCT "37777777777"
#define TEST_ADDR_MAX_HEX "ffffffff"
#define TEST_ADDR_MAX_HEX_UPPER "FFFFFFFF"
#endif

#if defined(USE_INT64)
typedef int64_t test_value_signed_t;
#define TEST_VALUE_MAX_DEC "18446744073709551615"
#define TEST_VALUE_MAX_OCT "1777777777777777777777"
#define TEST_VALUE_MAX_HEX "ffffffffffffffff"
#define TEST_VALUE_MAX_HEX_UPPER "FFFFFFFFFFFFFFFF"
#else
typedef int32_t test_value_signed_t;
#define TEST_VALUE_MAX_DEC "4294967295"
#define TEST_VALUE_MAX_OCT "37777777777"
#define TEST_VALUE_MAX_HEX "ffffffff"
#define TEST_VALUE_MAX_HEX_UPPER "FFFFFFFF"
#endif

static void test_addr_format_specifiers(void **state)
{
    const t_addr max_addr = (t_addr) ~(t_addr)0;

    (void)state;

    ASSERT_FORMAT(TEST_ADDR_MAX_DEC, "%" PRIuADDR, max_addr);
    ASSERT_FORMAT(TEST_ADDR_MAX_OCT, "%" PRIoADDR, max_addr);
    ASSERT_FORMAT(TEST_ADDR_MAX_HEX, "%" PRIxADDR, max_addr);
    ASSERT_FORMAT(TEST_ADDR_MAX_HEX_UPPER, "%" PRIXADDR, max_addr);
    ASSERT_FORMAT("-12345", "%" PRIdADDR, (test_addr_signed_t)-12345);
    ASSERT_FORMAT("-12345", "%" PRIiADDR, (test_addr_signed_t)-12345);
}

static void test_value_format_specifiers(void **state)
{
    const t_value max_value = (t_value) ~(t_value)0;

    (void)state;

    ASSERT_FORMAT(TEST_VALUE_MAX_DEC, "%" PRIuVALUE, max_value);
    ASSERT_FORMAT(TEST_VALUE_MAX_OCT, "%" PRIoVALUE, max_value);
    ASSERT_FORMAT(TEST_VALUE_MAX_HEX, "%" PRIxVALUE, max_value);
    ASSERT_FORMAT(TEST_VALUE_MAX_HEX_UPPER, "%" PRIXVALUE, max_value);
    ASSERT_FORMAT("-67890", "%" PRIdVALUE, (test_value_signed_t)-67890);
    ASSERT_FORMAT("-67890", "%" PRIiVALUE, (test_value_signed_t)-67890);
}

static void test_svalue_format_specifiers(void **state)
{
    const t_svalue signed_value = (t_svalue)-24680;
    const t_svalue hex_value = (t_svalue)0x1234;

    (void)state;

    ASSERT_FORMAT("-24680", "%" PRIdSVALUE, signed_value);
    ASSERT_FORMAT("-24680", "%" PRIiSVALUE, signed_value);
    ASSERT_FORMAT("4660", "%" PRIuSVALUE, hex_value);
    ASSERT_FORMAT("11064", "%" PRIoSVALUE, hex_value);
    ASSERT_FORMAT("1234", "%" PRIxSVALUE, hex_value);
    ASSERT_FORMAT("1234", "%" PRIXSVALUE, hex_value);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_addr_format_specifiers),
        cmocka_unit_test(test_value_format_specifiers),
        cmocka_unit_test(test_svalue_format_specifiers),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
