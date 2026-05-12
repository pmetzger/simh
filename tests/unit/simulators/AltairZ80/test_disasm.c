#include <string.h>

#include "test_cmocka.h"

#include "sim_types.h"

long disasm(uchar_t *data, char *output, size_t output_size, int segsize,
            long offset);

struct disasm_case {
    const char *name;
    uchar_t data[8];
    int segsize;
    long offset;
    long expected_length;
    const char *expected_output;
};

static void test_disasm_current_output_shapes(void **state)
{
    const struct disasm_case cases[] = {
        {"no match", {0x0f, 0x04}, 16, 0x100, 1, "db 00fh"},
        {"nop", {0x90}, 16, 0x100, 1, "nop"},
        {"lock prefix", {0xf0, 0x90}, 16, 0x100, 2, "lock nop"},
        {"rep prefix", {0xf3, 0xa4}, 16, 0x100, 2, "rep movsb"},
        {"immediate", {0xb8, 0x34, 0x12}, 16, 0x100, 3, "mov ax,0x1234"},
        {"bp displacement",
         {0x8b, 0x46, 0xfc},
         16,
         0x100,
         3,
         "mov ax,[bp-0x4]"},
        {"unused segment override", {0x2e, 0x90}, 16, 0x100, 2, "cs nop"},
        {"memory segment override",
         {0x2e, 0x8b, 0x06, 0x34, 0x12},
         16,
         0x100,
         5,
         "mov ax,[cs:0x1234]"},
        {"relative short jump", {0xeb, 0xfe}, 16, 0x100, 2, "jmp short 0x100"},
        {"address size prefix",
         {0x67, 0x8b, 0x00},
         16,
         0x100,
         3,
         "mov ax,[eax]"},
        {"operand size prefix",
         {0x66, 0xb8, 0x78, 0x56, 0x34, 0x12},
         16,
         0x100,
         6,
         "mov eax,0x12345678"},
    };

    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char output[128];
        uchar_t data[8];
        long length;

        memset(output, 0xa5, sizeof(output));
        memcpy(data, cases[i].data, sizeof(data));

        length = disasm(data, output, sizeof(output), cases[i].segsize,
                        cases[i].offset);

        assert_int_equal(length, cases[i].expected_length);
        assert_string_equal(output, cases[i].expected_output);
    }
}

static void test_disasm_respects_output_size(void **state)
{
    struct {
        char output[4];
        char guard[4];
    } buffer;
    const char expected_guard[] = {0x11, 0x22, 0x33, 0x44};
    uchar_t data[8] = {0x66, 0xb8, 0x78, 0x56, 0x34, 0x12};
    long length;

    (void)state;

    memset(&buffer, 0xa5, sizeof(buffer));
    memcpy(buffer.guard, expected_guard, sizeof(expected_guard));

    length = disasm(data, buffer.output, sizeof(buffer.output), 16, 0x100);

    assert_int_equal(length, 6);
    assert_string_equal(buffer.output, "mov");
    assert_memory_equal(buffer.guard, expected_guard, sizeof(expected_guard));
}

static void test_disasm_accepts_zero_output_size(void **state)
{
    char output = (char)0xa5;
    uchar_t data[8] = {0x90};
    long length;

    (void)state;

    length = disasm(data, &output, 0, 16, 0x100);

    assert_int_equal(length, 1);
    assert_int_equal((unsigned char)output, 0xa5);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_disasm_current_output_shapes),
        cmocka_unit_test(test_disasm_respects_output_size),
        cmocka_unit_test(test_disasm_accepts_zero_output_size),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
