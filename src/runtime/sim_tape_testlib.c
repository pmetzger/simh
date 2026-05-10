/* sim_tape_testlib.c: built-in TESTLIB coverage for simulator tape support */
// SPDX-FileCopyrightText: 1993-2008 Robert M Supnik
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: X11

#include <errno.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim_defs.h"
#include "sim_dynstr.h"
#include "sim_fio.h"
#include "sim_tape.h"
#include "sim_tape_internal.h"

static bool p7b_parity_inited = false;
static uint8_t p7b_odd_parity[64];
static uint8_t p7b_even_parity[64];

/* Build one allocated TESTLIB tape file name; caller frees result. */
static char *sim_tape_test_file_name(const char *filename, const char *suffix)
{
    return sim_dynstr_concat_cstrs(filename, suffix);
}

/* Open one sim_tape self-test file using an allocated suffix path. */
static t_stat sim_tape_test_open_file(FILE **stream, const char *filename,
                                      const char *suffix)
{
    char *name;

    name = sim_tape_test_file_name(filename, suffix);
    if (name == NULL)
        return SCPE_MEM;
    *stream = fopen(name, "wb");
    free(name);
    if (*stream == NULL)
        return SCPE_OPENERR;
    return SCPE_OK;
}

/* Open each TESTLIB tape file entry using its filename suffix. */
t_stat sim_tape_test_open_files(const SIM_TAPE_TEST_FILE *files,
                                size_t file_count, const char *filename)
{
    size_t i;

    for (i = 0; i < file_count; i++) {
        t_stat stat;

        stat =
            sim_tape_test_open_file(files[i].stream, filename, files[i].suffix);
        if (stat != SCPE_OK) {
            sim_tape_test_close_files(files, i);
            return stat;
        }
    }
    return SCPE_OK;
}

/* Close each open TESTLIB tape file entry and clear its stream pointer. */
void sim_tape_test_close_files(const SIM_TAPE_TEST_FILE *files,
                               size_t file_count)
{
    size_t i;

    for (i = 0; i < file_count; i++) {
        if (*files[i].stream != NULL) {
            fclose(*files[i].stream);
            *files[i].stream = NULL;
        }
    }
}

/* Remove one sim_tape self-test file using an allocated suffix path. */
static t_stat sim_tape_test_remove_file(const char *filename,
                                        const char *suffix)
{
    char *name;

    name = sim_tape_test_file_name(filename, suffix);
    if (name == NULL)
        return SCPE_MEM;
    (void)remove(name);
    free(name);
    return SCPE_OK;
}

/* Build TESTLIB Architecture Workstation tape attach args; caller frees. */
char *sim_tape_test_aws_attach_args(const char *filename)
{
    sim_dynstr_t args;

    if (filename == NULL)
        return NULL;

    sim_dynstr_init(&args);
    if (!sim_dynstr_appendf(&args, "aws %s.aws.tape", filename)) {
        sim_dynstr_free(&args);
        return NULL;
    }
    return sim_dynstr_take(&args);
}

/* Build one quoted TESTLIB tape file name; caller frees result. */
static char *sim_tape_test_quoted_file_name(const char *filename,
                                            const char *suffix)
{
    sim_dynstr_t name;

    if ((filename == NULL) || (suffix == NULL))
        return NULL;

    sim_dynstr_init(&name);
    if (!sim_dynstr_appendf(&name, "\"%s%s\"", filename, suffix)) {
        sim_dynstr_free(&name);
        return NULL;
    }
    return sim_dynstr_take(&name);
}

/* Build TESTLIB tape classification attach args; caller frees result. */
char *sim_tape_test_classify_args(const char *unit_name,
                                  const char *attach_args,
                                  const char *test_name)
{
    sim_dynstr_t args;

    if ((unit_name == NULL) || (attach_args == NULL) || (test_name == NULL))
        return NULL;

    sim_dynstr_init(&args);
    if (!sim_dynstr_appendf(&args, "%s -v %s %s", unit_name, attach_args,
                            test_name)) {
        sim_dynstr_free(&args);
        return NULL;
    }
    return sim_dynstr_take(&args);
}

static t_stat sim_tape_test_create_tape_files(UNIT *uptr, const char *filename,
                                              int files, int records,
                                              int max_size)
{
    FILE *fSIMH = NULL;
    FILE *fE11 = NULL;
    FILE *fTPC = NULL;
    FILE *fP7B = NULL;
    FILE *fAWS = NULL;
    FILE *fAWS2 = NULL;
    FILE *fAWS3 = NULL;
    FILE *fTAR = NULL;
    FILE *fTAR2 = NULL;
    FILE *fBIN = NULL;
    FILE *fTXT = NULL;
    FILE *fVAR = NULL;
    const SIM_TAPE_TEST_FILE tape_files[] = {
        {&fSIMH, ".simh"},     {&fE11, ".e11"},       {&fTPC, ".tpc"},
        {&fP7B, ".p7b"},       {&fTAR, ".tar"},       {&fTAR2, ".2.tar"},
        {&fAWS, ".aws"},       {&fAWS2, ".2.aws"},    {&fAWS3, ".3.aws"},
        {&fBIN, ".bin.fixed"}, {&fTXT, ".txt.fixed"}, {&fVAR, ".txt.ansi-var"},
    };
    int i;
    size_t j, k;
    t_tpclnt tpclnt;
    t_mtrlnt mtrlnt;
    t_awslnt awslnt;
    t_awslnt awslnt_last = 0;
    t_awslnt awsrec_typ = AWS_REC;
    t_stat stat = SCPE_OPENERR;
    uint8_t *buf = NULL, zpad = 0;
    t_stat aws_stat = MTSE_UNATT;
    char *aws_args = NULL;
    int32_t saved_switches = sim_switches;

    const char hello_world[] = "      WRITE (6,7)                              "
                               "                         HELLO001\r\n"
                               "    7 FORMAT(14H HELLO, WORLD!)                "
                               "                         HELLO002\r\n"
                               "      STOP                                     "
                               "                         HELLO003\r\n"
                               "      END                                      "
                               "                         HELLO004\r\n";

    srand(0); /* All devices use the same random sequence for binary data */
    if (max_size == 0)
        max_size = SIM_TAPE_MAX_RECORD_SIZE;
    if (!p7b_parity_inited) {
        for (i = 0; i < 64; i++) {
            int bit_count = 0;

            for (j = 0; j < 6; j++) {
                if (i & (1 << j))
                    ++bit_count;
            }
            p7b_odd_parity[i] = i | ((~bit_count & 1) << 6);
            p7b_even_parity[i] = i | ((bit_count & 1) << 6);
        }
        p7b_parity_inited = true;
    }
    buf = (uint8_t *)malloc(65536);
    if (buf == NULL)
        return SCPE_MEM;
    stat = sim_tape_test_open_files(
        tape_files, sizeof(tape_files) / sizeof(tape_files[0]), filename);
    if (stat != SCPE_OK)
        goto Done_Files;
    aws_args = sim_tape_test_aws_attach_args(filename);
    if (aws_args == NULL) {
        stat = SCPE_MEM;
        goto Done_Files;
    }
    sim_switches = SWMASK('F') | (sim_switches & SWMASK('D')) | SWMASK('N');
    if (sim_switches & SWMASK('D'))
        uptr->dctrl = MTSE_DBG_STR | MTSE_DBG_DAT;
    aws_stat = sim_tape_attach_ex(
        uptr, aws_args,
        (saved_switches & SWMASK('D')) ? MTSE_DBG_STR | MTSE_DBG_DAT : 0, 0);
    if (aws_stat != MTSE_OK) {
        stat = aws_stat;
        goto Done_Files;
    }
    sim_switches = saved_switches;
    stat = SCPE_OK;
    for (i = 0; i < files; i++) {
        size_t rec_size = 1 + (rand() % max_size);

        awslnt = (t_awslnt)rec_size;
        mtrlnt = (t_mtrlnt)rec_size;
        tpclnt = (t_tpclnt)rec_size;
        /* records: should ensure that records > 0, otherwise, this test becomes
         * interesting. */
        for (j = 0; j < (size_t)records; j++) {
            awsrec_typ = AWS_REC;
            if (sim_switches & SWMASK('V'))
                sim_printf("Writing %zu byte record\n", rec_size);
            for (k = 0; k < rec_size; k++)
                buf[k] = rand() & 0xFF;
            (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, fSIMH);
            (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, fE11);
            (void)sim_fwrite(&tpclnt, sizeof(tpclnt), 1, fTPC);
            (void)sim_fwrite(&awslnt, sizeof(awslnt), 1, fAWS);
            (void)sim_fwrite(&awslnt_last, sizeof(awslnt_last), 1, fAWS);
            (void)sim_fwrite(&awsrec_typ, sizeof(awsrec_typ), 1, fAWS);
            if (i == 0) {
                (void)sim_fwrite(&awslnt, sizeof(awslnt), 1, fAWS3);
                (void)sim_fwrite(&awslnt_last, sizeof(awslnt_last), 1, fAWS3);
                (void)sim_fwrite(&awsrec_typ, sizeof(awsrec_typ), 1, fAWS3);
            }
            awslnt_last = awslnt;
            (void)sim_fwrite(buf, 1, rec_size, fSIMH);
            (void)sim_fwrite(buf, 1, rec_size, fE11);
            (void)sim_fwrite(buf, 1, rec_size, fTPC);
            (void)sim_fwrite(buf, 1, rec_size, fAWS);
            if (i == 0)
                (void)sim_fwrite(buf, 1, rec_size, fAWS3);
            stat = sim_tape_wrrecf(uptr, buf, (t_mtrlnt)rec_size);
            if (MTSE_OK != stat)
                goto Done_Files;
            if (rec_size & 1) {
                (void)sim_fwrite(&zpad, 1, 1, fSIMH);
                (void)sim_fwrite(&zpad, 1, 1, fTPC);
            }
            (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, fSIMH);
            (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, fE11);
            for (k = 0; k < rec_size; k++)
                buf[k] =
                    p7b_odd_parity[buf[k] &
                                   0x3F]; /* Only 6 data bits plus parity */
            buf[0] |= P7B_SOR;
            (void)sim_fwrite(buf, 1, rec_size, fP7B);
        }
        awslnt_last = awslnt;
        mtrlnt = tpclnt = awslnt = 0;
        awsrec_typ = AWS_TMK;
        (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, fSIMH);
        (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, fE11);
        (void)sim_fwrite(&tpclnt, sizeof(tpclnt), 1, fTPC);
        buf[0] = P7B_SOR | P7B_EOF;
        (void)sim_fwrite(buf, 1, 1, fP7B);
        (void)sim_fwrite(&awslnt, sizeof(awslnt), 1, fAWS);
        (void)sim_fwrite(&awslnt_last, sizeof(awslnt_last), 1, fAWS);
        (void)sim_fwrite(&awsrec_typ, sizeof(awsrec_typ), 1, fAWS);
        if (i == 0) {
            (void)sim_fwrite(&awslnt, sizeof(awslnt), 1, fAWS3);
            (void)sim_fwrite(&awslnt_last, sizeof(awslnt_last), 1, fAWS3);
            (void)sim_fwrite(&awsrec_typ, sizeof(awsrec_typ), 1, fAWS3);
        }
        awslnt_last = 0;
        stat = sim_tape_wrtmk(uptr);
        if (MTSE_OK != stat)
            goto Done_Files;
        if (i == 0) {
            mtrlnt = MTR_GAP;
            for (j = 0; j < rec_size; j++)
                (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, fSIMH);
            mtrlnt = 0;
        }
    }
    mtrlnt = tpclnt = 0;
    (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, fSIMH);
    (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, fE11);
    (void)sim_fwrite(&tpclnt, sizeof(tpclnt), 1, fTPC);
    awslnt_last = awslnt;
    awsrec_typ = AWS_TMK;
    (void)sim_fwrite(&awslnt, sizeof(awslnt), 1, fAWS);
    (void)sim_fwrite(&awslnt_last, sizeof(awslnt_last), 1, fAWS);
    (void)sim_fwrite(&awsrec_typ, sizeof(awsrec_typ), 1, fAWS);
    mtrlnt = 0xffffffff;
    tpclnt = 0xffff;
    (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, fSIMH);
    (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, fE11);
    (void)sim_fwrite(&tpclnt, sizeof(tpclnt), 1, fTPC);
    (void)sim_fwrite(buf, 1, 1, fP7B);
    /* Write an unmatched record delimiter (aka garbage) at
       the end of the SIMH, E11 and AWS files */
    mtrlnt = 25;
    (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, fSIMH);
    (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, fE11);
    (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, fAWS);
    (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, fAWS);
    (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, fAWS);
    (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, uptr->fileref);
    (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, uptr->fileref);
    (void)sim_fwrite(&mtrlnt, sizeof(mtrlnt), 1, uptr->fileref);
    for (j = 0; j < (size_t)records; j++) {
        memset(buf, j, 10240);
        (void)sim_fwrite(buf, 1, 10240, fTAR);
        (void)sim_fwrite(buf, 1, 10240, fTAR2);
        (void)sim_fwrite(buf, 1, 10240, fBIN);
    }
    memset(buf, j, 10240);
    (void)sim_fwrite(buf, 1, 5120, fTAR2);
    for (j = 0; j < 3; j++) {
        awslnt_last = awslnt = 0;
        awsrec_typ = AWS_TMK;
        (void)sim_fwrite(&awslnt, sizeof(awslnt), 1, fAWS2);
        (void)sim_fwrite(&awslnt_last, sizeof(awslnt_last), 1, fAWS2);
        (void)sim_fwrite(&awsrec_typ, sizeof(awsrec_typ), 1, fAWS2);
    }
    (void)sim_fwrite(hello_world, 1, strlen(hello_world), fTXT);
    (void)sim_fwrite(hello_world, 1, strlen(hello_world), fVAR);
Done_Files:
    sim_tape_test_close_files(tape_files,
                              sizeof(tape_files) / sizeof(tape_files[0]));
    free(aws_args);
    free(buf);
    if (aws_stat == MTSE_OK)
        sim_tape_detach(uptr);
    if (stat == SCPE_OK) {
        char *name1;
        char *name2;

        name1 = sim_tape_test_quoted_file_name(filename, ".aws");
        name2 = sim_tape_test_quoted_file_name(filename, ".aws.tape");
        if ((name1 == NULL) || (name2 == NULL))
            stat = SCPE_MEM;
        else {
            sim_switches = SWMASK('F');
            if (sim_cmp_string(name1, name2))
                stat = 1;
        }
        free(name1);
        free(name2);
    }
    sim_switches = saved_switches;
    return stat;
}

/* Build TESTLIB tape attach arguments for one processing step; caller frees. */
char *sim_tape_test_process_args(const char *filename, const char *format,
                                 t_awslnt recsize)
{
    sim_dynstr_t args;

    if ((filename == NULL) || (format == NULL))
        return NULL;

    sim_dynstr_init(&args);
    if (!sim_dynstr_append(&args, format))
        goto fail;
    if (recsize != 0) {
        if (!sim_dynstr_appendf(&args, " %d", (int)recsize))
            goto fail;
    }
    if (strchr(filename, '*') == NULL) {
        if (!sim_dynstr_appendf(&args, " %s.%s", filename, format))
            goto fail;
    } else {
        if (!sim_dynstr_appendf(&args, " %s", filename))
            goto fail;
    }

    return sim_dynstr_take(&args);

fail:
    sim_dynstr_free(&args);
    return NULL;
}

static t_stat sim_tape_test_process_tape_file(UNIT *uptr, const char *filename,
                                              const char *format,
                                              t_awslnt recsize)
{
    char *args;
    int32_t saved_switches = sim_switches;
    t_stat stat;

    if (recsize != 0)
        sim_switches |= SWMASK('B');
    args = sim_tape_test_process_args(filename, format, recsize);
    if (args == NULL) {
        sim_switches = saved_switches;
        return SCPE_MEM;
    }
    sim_tape_detach(uptr);
    sim_switches |= SWMASK('F') | SWMASK('L');
    stat = sim_tape_attach_ex(uptr, args, 0, 0);
    free(args);
    if (stat != SCPE_OK) {
        sim_switches = saved_switches;
        return stat;
    }
    sim_tape_detach(uptr);
    sim_switches = 0;
    return SCPE_OK;
}

static t_stat sim_tape_test_remove_tape_files(const char *filename)
{
    static const char *const suffixes[] = {
        ".simh",         ".2.simh",   ".e11",   ".2.e11",     ".tpc",
        ".2.tpc",        ".p7b",      ".2.p7b", ".aws",       ".2.aws",
        ".3.aws",        ".tar",      ".2.tar", ".bin.fixed", ".txt.fixed",
        ".txt.ansi-var", ".aws.tape",
    };
    size_t i;

    for (i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        t_stat stat = sim_tape_test_remove_file(filename, suffixes[i]);

        if (stat != SCPE_OK)
            return stat;
    }
    return SCPE_OK;
}

static t_stat sim_tape_test_density_string(void)
{
    char buf[128];
    int32_t valid_bits = 0;
    t_stat stat;

    if ((SCPE_ARG !=
         (stat = sim_tape_density_supported(buf, sizeof(buf), valid_bits))) ||
        (strcmp(buf, "")))
        return stat;
    valid_bits = MT_556_VALID;
    if ((SCPE_OK !=
         (stat = sim_tape_density_supported(buf, sizeof(buf), valid_bits))) ||
        (strcmp(buf, "556")))
        return sim_messagef(SCPE_ARG, "stat was: %s, got string: %s\n",
                            sim_error_text(stat), buf);
    valid_bits = MT_800_VALID | MT_1600_VALID;
    if ((SCPE_OK !=
         (stat = sim_tape_density_supported(buf, sizeof(buf), valid_bits))) ||
        (strcmp(buf, "{800|1600}")))
        return sim_messagef(SCPE_ARG, "stat was: %s, got string: %s\n",
                            sim_error_text(stat), buf);
    valid_bits = MT_800_VALID | MT_1600_VALID | MT_6250_VALID;
    if ((SCPE_OK !=
         (stat = sim_tape_density_supported(buf, sizeof(buf), valid_bits))) ||
        (strcmp(buf, "{800|1600|6250}")))
        return sim_messagef(SCPE_ARG, "stat was: %s, got string: %s\n",
                            sim_error_text(stat), buf);
    valid_bits = MT_200_VALID | MT_800_VALID | MT_1600_VALID | MT_6250_VALID;
    if ((SCPE_OK !=
         (stat = sim_tape_density_supported(buf, sizeof(buf), valid_bits))) ||
        (strcmp(buf, "{200|800|1600|6250}")))
        return sim_messagef(SCPE_ARG, "stat was: %s, got string: %s\n",
                            sim_error_text(stat), buf);
    valid_bits = MT_NONE_VALID | MT_800_VALID | MT_1600_VALID | MT_6250_VALID;
    if ((SCPE_OK !=
         (stat = sim_tape_density_supported(buf, sizeof(buf), valid_bits))) ||
        (strcmp(buf, "{0|800|1600|6250}")))
        return sim_messagef(SCPE_ARG, "stat was: %s, got string: %s\n",
                            sim_error_text(stat), buf);
    return SCPE_OK;
}

static struct classify_test {
    const char *testname;
    const char *testdata;
    size_t expected_mrs;
    bool expected_lf_lines;
    bool expected_crlf_lines;
    const char *success_attach_args;
    const char *fail_attach_args;
} classify_tests[] = {{"TapeTest-Classify-80.txt",
                       "Now is the time for all good men to come to the aid of "
                       "their country.~~~~~~~~~~~\r\n",
                       80, false, true, "-fb FIXED 80"},
                      {"TapeTest-Classify-80-lf.txt",
                       "Now is the time for all good men to come to the aid of "
                       "their country.~~~~~~~~~~~\n",
                       80, true, false, "-fb FIXED 80"},
                      {"TapeTest-Classify-508.txt",
                       "A really long line of text (512 - 4 = 508 characters) "
                       "64646464641281281281281281"
                       "2812812812812812812812812812812812812812812812812562562"
                       "5625625625625625625625625"
                       "6256256256256256256256256256256225625625625625625625625"
                       "6256256256256256256256256"
                       "2562562562562562512512512512512512512512512512512512512"
                       "5125125125125125125125125"
                       "5125125125125125125125125125125125125125125125125125125"
                       "1251251255125125125125125"
                       "1251251251251251251251251251251251251251251251255125125"
                       "1251251251251251251251251"
                       "2512512512512512512512512512\r\n",
                       508, false, true, "-fb FIXED 512"},
                      {"TapeTest-Classify-512.txt",
                       "A really long line of text (516 - 4 = 512 characters) "
                       "64646464641281281281281281"
                       "2812812812812812812812812812812812812812812812812562562"
                       "5625625625625625625625625"
                       "6256256256256256256256256256256225625625625625625625625"
                       "6256256256256256256256256"
                       "2562562562562562512512512512512512512512512512512512512"
                       "5125125125125125125125125"
                       "5125125125125125125125125125125125125125125125125125125"
                       "1251251255125125125125125"
                       "1251251251251251251251251251251251251251251251255125125"
                       "1251251251251251251251251"
                       "2512512512512512512512512512~~~~\r\n",
                       512, false, true, "-fb FIXED 512", "-fb ANSI-VMS 512"},
                      {"TapeTest-Classify-82.bin",
                       "Now is the time for all good men to come to the aid of "
                       "their country.\001\002~~~~~~~~~\r\n"
                       "Now is the time for all good men to come to the aid of "
                       "their country.\001\002~~~~~~~~~\r\n",
                       512, false, false, "-fb FIXED 82"},
                      {NULL}};

static t_stat sim_tape_test_classify_file_contents(UNIT *uptr)
{
    struct classify_test *t;
    FILE *f;
    size_t mrs;
    bool lf_lines;
    bool crlf_lines;

    for (t = classify_tests; t->testname != NULL; t++) {
        (void)remove(t->testname);
        f = fopen(t->testname, "wb+");
        if (f == NULL)
            return sim_messagef(SCPE_ARG,
                                "Error creating test file '%s' - %s\n",
                                t->testname, strerror(errno));
        fprintf(f, "%s", t->testdata);
        sim_tape_classify_file_contents(f, &mrs, &lf_lines, &crlf_lines);
        fclose(f);
        if ((mrs != t->expected_mrs) || (lf_lines != t->expected_lf_lines) ||
            (crlf_lines != t->expected_crlf_lines))
            return sim_messagef(SCPE_ARG,
                                "%s was unexpectedly reported to having "
                                "MRS=%d, lf_lines=%s, crlf_lines=%s\n",
                                t->testname, (int)mrs,
                                lf_lines ? "true" : "false",
                                crlf_lines ? "true" : "false");
        if (t->success_attach_args) {
            char *args;
            t_stat r;

            args = sim_tape_test_classify_args(
                sim_uname(uptr), t->success_attach_args, t->testname);
            if (args == NULL)
                return SCPE_MEM;
            r = attach_cmd(0, args);
            if (r != SCPE_OK) {
                t_stat stat;

                stat = sim_messagef(r, "ATTACH %s failed\n", args);
                free(args);
                return stat;
            }
            detach_cmd(0, sim_uname(uptr));
            free(args);
        }
        if (t->fail_attach_args) {
            char *args;
            t_stat r;

            args = sim_tape_test_classify_args(
                sim_uname(uptr), t->fail_attach_args, t->testname);
            if (args == NULL)
                return SCPE_MEM;
            r = attach_cmd(0, args);
            if (r == SCPE_OK) {
                t_stat stat;

                detach_cmd(0, sim_uname(uptr));
                stat = sim_messagef(r, "** UNEXPECTED ATTACH SUCCESS ** %s\n",
                                    args);
                free(args);
                return stat;
            }
            free(args);
        }
        (void)remove(t->testname);
    }
    return SCPE_OK;
}

t_stat sim_tape_test(DEVICE *dptr, const char *cptr)
{
    static const struct {
        const char *filename;
        const char *format;
        t_awslnt recsize;
    } process_tests[] = {
        {"TapeTestFile1.bin", "fixed", 2048},
        {"TapeTestFile1.txt", "fixed", 80},
        {"TapeTestFile1.*", "dos11", 0},
        {"TapeTestFile1.*", "ansi-vms", 0},
        {"TapeTestFile1.*", "ansi-rsx11", 0},
        {"TapeTestFile1.*", "ansi-rt11", 0},
        {"TapeTestFile1.*", "ansi-rsts", 0},
        {"TapeTestFile1.txt", "ansi-var", 0},
        {"TapeTestFile1", "tar", 0},
        {"TapeTestFile1.2", "tar", 0},
        {"TapeTestFile1", "aws", 0},
        {"TapeTestFile1.2", "aws", 0},
        {"TapeTestFile1.3", "aws", 0},
        {"TapeTestFile1", "p7b", 0},
        {"TapeTestFile1", "tpc", 0},
        {"TapeTestFile1", "e11", 0},
        {"TapeTestFile1", "simh", 0},
    };
    int32_t saved_switches = sim_switches;
    size_t i;
    SIM_TEST_INIT;

    /* Generic callback signature.
       This implementation does not use every parameter. */
    (void)cptr;

    if (dptr->units->flags & UNIT_ATT)
        return sim_messagef(SCPE_ALATT,
                            "The %s device must be detached to run the "
                            "sim_tape library API tests.\n",
                            sim_uname(dptr->units));

    sim_printf("\nTesting %s device sim_tape library APIs\n",
               sim_uname(dptr->units));

    SIM_TEST(sim_tape_test_density_string());

    SIM_TEST(sim_tape_test_classify_file_contents(dptr->units));

    SIM_TEST(sim_tape_test_remove_tape_files("TapeTestFile1"));

    SIM_TEST(sim_tape_test_create_tape_files(dptr->units, "TapeTestFile1", 2, 5,
                                             4096));

    for (i = 0; i < sizeof(process_tests) / sizeof(process_tests[0]); i++) {
        sim_switches = saved_switches;
        SIM_TEST(sim_tape_test_process_tape_file(
            dptr->units, process_tests[i].filename, process_tests[i].format,
            process_tests[i].recsize));
    }

    sim_switches = saved_switches;
    if ((sim_switches & SWMASK('D')) == 0)
        SIM_TEST(sim_tape_test_remove_tape_files("TapeTestFile1"));

    return SCPE_OK;
}
