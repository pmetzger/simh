/* test_kx10_imp_ftp.c: PDP-10 IMP FTP helper tests */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "kx10_imp_ftp.h"

static void test_ftp_port_rewrite_preserves_port_suffix(void **state)
{
    static const uint8_t payload[] = "PORT 10,0,0,1,7,138\r\n";
    char out[64];
    size_t out_len = 0;

    (void)state;

    assert_true(kx10_imp_format_ftp_port(out, sizeof(out), 0xc0000205u, payload,
                                         strlen((const char *)payload),
                                         &out_len));
    assert_string_equal(out, "PORT 192,0,2,5,7,138\r\n");
    assert_int_equal(out_len, strlen(out));
}

static void test_ftp_port_rewrite_preserves_binary_suffix(void **state)
{
    static const uint8_t payload[] = {'P', 'O', 'R', 'T', ' ',  '1',
                                      ',', '2', ',', '3', ',',  '4',
                                      ',', '9', ',', '1', '\0', 'X'};
    char out[64];
    size_t out_len = 0;
    const char expected[] = "PORT 127,0,0,1,9,1";

    (void)state;

    assert_true(kx10_imp_format_ftp_port(out, sizeof(out), 0x7f000001u, payload,
                                         sizeof(payload), &out_len));
    assert_memory_equal(out, expected, strlen(expected));
    assert_memory_equal(out + strlen(expected), "\0X", 2);
    assert_int_equal(out_len, strlen(expected) + 2);
    assert_int_equal(out[out_len], '\0');
}

static void test_ftp_port_rewrite_rejects_short_command(void **state)
{
    static const uint8_t payload[] = "PORT 10,0,0";
    char out[64] = "unchanged";
    size_t out_len = 123;

    (void)state;

    assert_false(
        kx10_imp_format_ftp_port(out, sizeof(out), 0xc0000205u, payload,
                                 strlen((const char *)payload), &out_len));
    assert_string_equal(out, "unchanged");
    assert_int_equal(out_len, 123);
}

static void test_ftp_port_rewrite_rejects_non_port_payload(void **state)
{
    static const uint8_t payload[] = "NOPE 10,0,0,1,7,138\r\n";
    char out[64] = "unchanged";
    size_t out_len = 123;

    (void)state;

    assert_false(
        kx10_imp_format_ftp_port(out, sizeof(out), 0xc0000205u, payload,
                                 strlen((const char *)payload), &out_len));
    assert_string_equal(out, "unchanged");
    assert_int_equal(out_len, 123);
}

static void test_ftp_port_rewrite_rejects_too_small_output(void **state)
{
    static const uint8_t payload[] = "PORT 10,0,0,1,7,138\r\n";
    char out[16] = "unchanged";
    size_t out_len = 123;

    (void)state;

    assert_false(
        kx10_imp_format_ftp_port(out, sizeof(out), 0xffffffffu, payload,
                                 strlen((const char *)payload), &out_len));
    assert_string_equal(out, "unchanged");
    assert_int_equal(out_len, 123);
}

static void test_ftp_port_rewrite_rejects_null_arguments(void **state)
{
    static const uint8_t payload[] = "PORT 10,0,0,1,7,138\r\n";
    char out[64] = "unchanged";
    size_t out_len = 123;

    (void)state;

    assert_false(kx10_imp_format_ftp_port(NULL, sizeof(out), 0xc0000205u,
                                          payload, sizeof(payload), &out_len));
    assert_false(kx10_imp_format_ftp_port(out, 0, 0xc0000205u, payload,
                                          sizeof(payload), &out_len));
    assert_false(kx10_imp_format_ftp_port(out, sizeof(out), 0xc0000205u, NULL,
                                          sizeof(payload), &out_len));
    assert_false(kx10_imp_format_ftp_port(out, sizeof(out), 0xc0000205u,
                                          payload, sizeof(payload), NULL));
    assert_string_equal(out, "unchanged");
    assert_int_equal(out_len, 123);
}

static void test_ftp_port_payload_rewrite_updates_buffer(void **state)
{
    uint8_t payload[64] = "PORT 10,0,0,1,7,138\r\n";
    size_t rewritten_len = 0;

    (void)state;

    assert_true(kx10_imp_rewrite_ftp_port_payload(
        payload, sizeof(payload), 0xc0000205u, strlen((const char *)payload),
        &rewritten_len));
    assert_string_equal((const char *)payload, "PORT 192,0,2,5,7,138\r\n");
    assert_int_equal(rewritten_len, strlen((const char *)payload));
}

static void test_ftp_port_payload_rewrite_leaves_buffer_on_failure(void **state)
{
    uint8_t payload[16] = "unchanged";
    size_t rewritten_len = 123;

    (void)state;

    assert_false(kx10_imp_rewrite_ftp_port_payload(
        payload, sizeof(payload), 0xffffffffu, strlen((const char *)payload),
        &rewritten_len));
    assert_string_equal((const char *)payload, "unchanged");
    assert_int_equal(rewritten_len, 123);
}

static void test_ftp_port_payload_rewrite_rejects_oversized_length(void **state)
{
    uint8_t payload[16] = "unchanged";
    size_t rewritten_len = 123;

    (void)state;

    assert_false(
        kx10_imp_rewrite_ftp_port_payload(payload, sizeof(payload), 0xc0000205u,
                                          sizeof(payload) + 1, &rewritten_len));
    assert_string_equal((const char *)payload, "unchanged");
    assert_int_equal(rewritten_len, 123);
}

static void test_ftp_port_payload_rewrite_allows_full_buffer(void **state)
{
    uint8_t payload[1500] = "PORT 99,0,0,1,";
    static const char expected_prefix[] = "PORT 10,0,0,1,";
    size_t prefix_len = strlen((const char *)payload);
    size_t rewritten_len = 0;

    (void)state;

    memset(payload + prefix_len, 'X', sizeof(payload) - prefix_len);
    assert_true(kx10_imp_rewrite_ftp_port_payload(payload, sizeof(payload),
                                                  0x0a000001u, sizeof(payload),
                                                  &rewritten_len));
    assert_int_equal(rewritten_len, sizeof(payload));
    assert_memory_equal(payload, expected_prefix, strlen(expected_prefix));
    assert_int_equal(payload[sizeof(payload) - 1], 'X');
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ftp_port_rewrite_preserves_port_suffix),
        cmocka_unit_test(test_ftp_port_rewrite_preserves_binary_suffix),
        cmocka_unit_test(test_ftp_port_rewrite_rejects_short_command),
        cmocka_unit_test(test_ftp_port_rewrite_rejects_non_port_payload),
        cmocka_unit_test(test_ftp_port_rewrite_rejects_too_small_output),
        cmocka_unit_test(test_ftp_port_rewrite_rejects_null_arguments),
        cmocka_unit_test(test_ftp_port_payload_rewrite_updates_buffer),
        cmocka_unit_test(
            test_ftp_port_payload_rewrite_leaves_buffer_on_failure),
        cmocka_unit_test(
            test_ftp_port_payload_rewrite_rejects_oversized_length),
        cmocka_unit_test(test_ftp_port_payload_rewrite_allows_full_buffer),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
