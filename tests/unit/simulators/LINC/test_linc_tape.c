#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "linc_defs.h"
#include "test_support.h"

#define LINC_DATA_WORDS 256
#define LINC_BLOCK_BYTES (LINC_DATA_WORDS * 2)

REG cpu_reg[13];
uint16_t M[MEMSIZE];

t_stat cpu_do(void)
{
    return SCPE_OK;
}

struct linc_tape_fixture {
    char temp_dir[1024];
    char image_path[1024];
};

static int setup_linc_tape_fixture(void **state)
{
    struct linc_tape_fixture *fixture;

    fixture = calloc(1, sizeof(*fixture));
    assert_non_null(fixture);

    assert_int_equal(simh_test_make_temp_dir(fixture->temp_dir,
                                             sizeof(fixture->temp_dir),
                                             "linc-tape"),
                     0);
    assert_int_equal(simh_test_join_path(fixture->image_path,
                                         sizeof(fixture->image_path),
                                         fixture->temp_dir, "image.linc"),
                     0);

    *state = fixture;
    return 0;
}

static int teardown_linc_tape_fixture(void **state)
{
    struct linc_tape_fixture *fixture = *state;

    assert_int_equal(simh_test_remove_path(fixture->temp_dir), 0);
    free(fixture);
    *state = NULL;
    return 0;
}

static void write_word(FILE *file, uint16_t word)
{
    uint8_t bytes[2];

    bytes[0] = word & 0xff;
    bytes[1] = (word >> 8) & 0x0f;
    assert_int_equal(fwrite(bytes, 1, sizeof(bytes), file), sizeof(bytes));
}

static void write_bad_word(FILE *file, uint16_t word)
{
    uint8_t bytes[2];

    bytes[0] = word & 0xff;
    bytes[1] = ((word >> 8) & 0x0f) | 0x80;
    assert_int_equal(fwrite(bytes, 1, sizeof(bytes), file), sizeof(bytes));
}

static FILE *open_fixture_image(struct linc_tape_fixture *fixture)
{
    FILE *file;

    file = fopen(fixture->image_path, "wb+");
    assert_non_null(file);
    return file;
}

static void write_zero_block(FILE *file)
{
    uint8_t zeroes[LINC_BLOCK_BYTES];

    memset(zeroes, 0, sizeof(zeroes));
    assert_int_equal(fwrite(zeroes, 1, sizeof(zeroes), file), sizeof(zeroes));
}

static void test_plain_image_metadata(void **state)
{
    struct linc_tape_fixture *fixture = *state;
    uint16_t block_size = 0;
    int16_t forward_offset = -1;
    int16_t reverse_offset = -1;
    FILE *file;

    file = open_fixture_image(fixture);
    for (size_t i = 0; i < 512; ++i) {
        write_zero_block(file);
    }
    rewind(file);

    assert_int_equal(
        tape_metadata(file, &block_size, &forward_offset, &reverse_offset),
        SCPE_OK);
    assert_int_equal(block_size, LINC_DATA_WORDS);
    assert_int_equal(forward_offset, 0);
    assert_int_equal(reverse_offset, 0);

    assert_int_equal(fclose(file), 0);
}

static void test_extended_image_metadata(void **state)
{
    struct linc_tape_fixture *fixture = *state;
    uint16_t block_size = 0;
    int16_t forward_offset = 0;
    int16_t reverse_offset = 0;
    FILE *file;

    file = open_fixture_image(fixture);
    write_zero_block(file);
    write_word(file, LINC_DATA_WORDS);
    write_word(file, 012);
    write_word(file, 012);
    rewind(file);

    assert_int_equal(
        tape_metadata(file, &block_size, &forward_offset, &reverse_offset),
        SCPE_OK);
    assert_int_equal(block_size, LINC_DATA_WORDS);
    assert_int_equal(forward_offset, 012);
    assert_int_equal(reverse_offset, 012);

    assert_int_equal(fclose(file), 0);
}

static void test_extended_image_metadata_rejects_high_bits(void **state)
{
    struct linc_tape_fixture *fixture = *state;
    uint16_t block_size = 0;
    int16_t forward_offset = 0;
    int16_t reverse_offset = 0;
    FILE *file;

    file = open_fixture_image(fixture);
    write_zero_block(file);
    write_bad_word(file, LINC_DATA_WORDS);
    write_word(file, 0);
    write_word(file, 0);
    rewind(file);

    assert_int_equal(
        tape_metadata(file, &block_size, &forward_offset, &reverse_offset),
        SCPE_FMT);

    assert_int_equal(fclose(file), 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_plain_image_metadata,
                                        setup_linc_tape_fixture,
                                        teardown_linc_tape_fixture),
        cmocka_unit_test_setup_teardown(test_extended_image_metadata,
                                        setup_linc_tape_fixture,
                                        teardown_linc_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_extended_image_metadata_rejects_high_bits,
            setup_linc_tape_fixture, teardown_linc_tape_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
