#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "test_cmocka.h"

#include "sim_tempfile.h"
#include "sim_defs.h"

void fprint_fields(FILE *stream, t_value before, t_value after,
                   BITFIELD *bitdefs);

static char *read_stream(FILE *stream)
{
    long size;
    char *buffer;

    assert_int_equal(fflush(stream), 0);
    assert_int_equal(fseek(stream, 0, SEEK_END), 0);
    size = ftell(stream);
    assert_true(size >= 0);
    assert_int_equal(fseek(stream, 0, SEEK_SET), 0);

    buffer = calloc((size_t)size + 1, 1);
    assert_non_null(buffer);
    assert_int_equal(fread(buffer, 1, (size_t)size, stream), (size_t)size);

    return buffer;
}

static void assert_fields_print(BITFIELD *fields, t_value before, t_value after,
                                const char *expected)
{
    FILE *stream;
    char *actual;

    stream = sim_tmpfile();
    assert_non_null(stream);

    fprint_fields(stream, before, after, fields);
    actual = read_stream(stream);

    assert_string_equal(actual, expected);

    free(actual);
    fclose(stream);
}

static void test_bitfield_display_formats(void **state)
{
    BITFIELD signed_fields[] = {BITF_SIGNED(V, 32), ENDBITS};
    BITFIELD unsigned_fields[] = {BITF_UNSIGNED(V, 8), ENDBITS};
    BITFIELD hex2_fields[] = {BITF_HEX2(V, 8), ENDBITS};
    BITFIELD hex2_prefix_fields[] = {BITF_HEX2P(V, 8), ENDBITS};
    BITFIELD hex4_prefix_fields[] = {BITF_HEX4P(V, 16), ENDBITS};
    BITFIELD quoted_hex2_fields[] = {BITF_QHEX2(V, 8), ENDBITS};
    BITFIELD quoted_octal_fields[] = {BITF_QOCTAL(V, 8), ENDBITS};
    BITFIELD quoted_unsigned_fields[] = {BITF_QUNSIGNED(V, 8), ENDBITS};
    BITFIELD quoted_hex_prefix_fields[] = {BITF_QHEXP(V, 8), ENDBITS};

    (void)state;

    assert_fields_print(signed_fields, 0xffffffff, 0xffffffff, "V=-1 ");
    assert_fields_print(unsigned_fields, 17, 17, "V=17 ");
    assert_fields_print(hex2_fields, 0x0a, 0x0a, "V=0A ");
    assert_fields_print(hex2_prefix_fields, 0x0a, 0x0a, "V=0x0A ");
    assert_fields_print(hex4_prefix_fields, 0x0a, 0x0a, "V=0x000A ");
    assert_fields_print(quoted_hex2_fields, 0x0a, 0x0a, "V=\"0A\" ");
    assert_fields_print(quoted_octal_fields, 8, 8, "V=\"10\" ");
    assert_fields_print(quoted_unsigned_fields, 17, 17, "V=\"17\" ");
    assert_fields_print(quoted_hex_prefix_fields, 0x0a, 0x0a, "V=\"0xA\" ");
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bitfield_display_formats),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
