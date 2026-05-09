/* test_ibm1130_cr.c: tests for IBM 1130 card reader behavior */

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#define access _access
#else
#include <unistd.h>
#endif

#include "test_cmocka.h"
#include "test_support.h"

#include "ibm1130_defs.h"
#include "ibm1130_fmt.h"
#include "sim_types.h"

UNIT cpu_unit = {0};
bool cgi = false;
bool cgiwritable = false;
bool sim_gui = false;
uint16_t M[MAXMEMSIZE] = {0};
uint16_t ILSW[6] = {0};
int32_t IAR = 0;
int32_t prev_IAR = 0;
int32_t SAR = 0;
int32_t SBR = 0;
int32_t OP = 0;
int32_t TAG = 0;
int32_t CCC = 0;
int32_t CES = 0;
int32_t ACC = 0;
int32_t EXT = 0;
int32_t ARF = 0;
int32_t RUNMODE = 0;
int32_t ipl = 0;
int32_t iplpending = 0;
int32_t tbit = 0;
int32_t V = 0;
int32_t C = 0;
int32_t wait_state = 0;
int32_t wait_lamp = 0;
int32_t int_req = 0;
int32_t int_lamps = 0;
int32_t int_mask = 0;
int32_t mem_mask = MAXMEMSIZE - 1;
int32_t cpu_dsw = 0;
int32_t con_dsw = 0;
bool running = false;
bool power = false;
t_stat reason = SCPE_OK;

static t_stat test_break_reason = SCPE_OK;

int32_t ReadW(int32_t a)
{
    (void)a;
    return 0;
}

void WriteW(int32_t a, int32_t d)
{
    (void)a;
    (void)d;
}

void calc_ints(void)
{
}

void break_simulation(t_stat stopreason)
{
    test_break_reason = stopreason;
}

void trace_io(const char *fmt, ...)
{
    (void)fmt;
}

void debug_print(const char *fmt, ...)
{
    (void)fmt;
}

void void_backtrace(int afrom, int ato)
{
    (void)afrom;
    (void)ato;
}

const char *saywhere(int addr)
{
    (void)addr;
    return "";
}

int strnicmp(const char *a, const char *b, size_t n)
{
    while (n-- != 0) {
        uchar_t ca = (uchar_t)*a++;
        uchar_t cb = (uchar_t)*b++;

        ca = (uchar_t)tolower(ca);
        cb = (uchar_t)tolower(cb);
        if (ca != cb)
            return ca - cb;
        if (ca == '\0')
            return 0;
    }

    return 0;
}

char *upcase(char *str)
{
    char *p;

    for (p = str; *p != '\0'; p++)
        *p = (char)toupper((uchar_t)*p);

    return str;
}

void xio_error(const char *msg)
{
    (void)msg;
}

void remark_cmd(char *remark)
{
    (void)remark;
}

const char *quotefix(const char *cptr, char *buf)
{
    (void)buf;
    return cptr;
}

const char *EditToAsm(char *str, int width)
{
    (void)width;
    return str;
}

const char *EditToFortran(char *str, int width)
{
    (void)width;
    return str;
}

const char *EditToWhitespace(char *str, int width)
{
    (void)width;
    return str;
}

#include "ibm1130_cr.c"

struct ibm1130_cr_fixture {
    char temp_dir[1024];
    FILE *deck;
};

static int setup_ibm1130_cr(void **state)
{
    struct ibm1130_cr_fixture *fixture = calloc(1, sizeof(*fixture));

    assert_non_null(fixture);
    assert_int_equal(simh_test_make_temp_dir(fixture->temp_dir,
                                             sizeof(fixture->temp_dir),
                                             "ibm1130-cr"),
                     0);
    assert_int_equal(setenv("TMPDIR", fixture->temp_dir, 1), 0);

    fixture->deck = tmpfile();
    assert_non_null(fixture->deck);

    deckfile = fixture->deck;
    tempfile[0] = '\0';
    cr_unit.fileref = NULL;
    cr_unit.flags = UNIT_QUIET | UNIT_CR_EMPTY;
    cr_unit.pos = 0;
    test_break_reason = SCPE_OK;

    *state = fixture;
    return 0;
}

static int teardown_ibm1130_cr(void **state)
{
    struct ibm1130_cr_fixture *fixture = *state;

    if (cr_unit.fileref != NULL) {
        fclose(cr_unit.fileref);
        cr_unit.fileref = NULL;
    }
    if (tempfile[0] != '\0')
        remove(tempfile);

    deckfile = NULL;
    tempfile[0] = '\0';
    cr_unit.flags = UNIT_CR_EMPTY;

    if (fixture->deck != NULL)
        fclose(fixture->deck);
    unsetenv("TMPDIR");
    assert_int_equal(simh_test_remove_path(fixture->temp_dir), 0);
    free(fixture);
    *state = NULL;
    return 0;
}

#if !defined(_WIN32)
static void assert_path_is_in_directory(const char *path, const char *dir)
{
    size_t dir_len = strlen(dir);

    assert_true(strncmp(path, dir, dir_len) == 0);
    assert_int_equal(path[dir_len], '/');
}
#endif

static void test_literal_deck_lines_create_scratch_deck(void **state)
{
    struct ibm1130_cr_fixture *fixture = *state;
    char line[32];

    assert_int_not_equal(fputs("!first\n!second\n", fixture->deck), EOF);
    assert_int_equal(fseek(fixture->deck, 0, SEEK_SET), 0);

    assert_true(nextdeck());
    assert_int_equal(test_break_reason, SCPE_OK);
    assert_true(cr_unit.flags & UNIT_SCRATCH);
    assert_non_null(cr_unit.fileref);
    assert_string_not_equal(tempfile, "");
    assert_int_equal(access(tempfile, F_OK), 0);
#if !defined(_WIN32)
    assert_path_is_in_directory(tempfile, fixture->temp_dir);
#endif

    assert_non_null(fgets(line, sizeof(line), cr_unit.fileref));
    assert_string_equal(line, "FIRST\n");
    assert_non_null(fgets(line, sizeof(line), cr_unit.fileref));
    assert_string_equal(line, "SECOND\n");
    assert_null(fgets(line, sizeof(line), cr_unit.fileref));
}

static void test_nextdeck_removes_scratch_deck_file(void **state)
{
    struct ibm1130_cr_fixture *fixture = *state;
    char scratch_path[PATH_MAX + 1];

    assert_int_not_equal(fputs("!scratch\n", fixture->deck), EOF);
    assert_int_equal(fseek(fixture->deck, 0, SEEK_SET), 0);

    assert_true(nextdeck());
    assert_int_equal(strlcpy(scratch_path, tempfile, sizeof(scratch_path)),
                     strlen(tempfile));
    assert_int_equal(access(scratch_path, F_OK), 0);

    assert_false(nextdeck());
    assert_false(cr_unit.flags & UNIT_SCRATCH);
    assert_null(cr_unit.fileref);
    assert_string_equal(tempfile, "");
    assert_int_not_equal(access(scratch_path, F_OK), 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_literal_deck_lines_create_scratch_deck, setup_ibm1130_cr,
            teardown_ibm1130_cr),
        cmocka_unit_test_setup_teardown(
            test_nextdeck_removes_scratch_deck_file, setup_ibm1130_cr,
            teardown_ibm1130_cr),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
