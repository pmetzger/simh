/* i8080_load.c: Intel 8080/8085 loader and dumper support */
// SPDX-FileCopyrightText: 1997-2005 Charles E. Owen
// SPDX-FileCopyrightText: 2011 William A. Beech
// SPDX-License-Identifier: X11

#include <stdbool.h>
#include "system_defs.h"

#define HLEN 16
#define IHEX_DATA 0
#define IHEX_EOF 1
#define IHEX_MAX_DATA 255

extern uint32 saved_PC;
extern uint8 get_mbyte(uint16 addr);
extern void put_mbyte(uint16 addr, uint8 val);

t_stat sim_load(FILE *fileref, const char *cptr, const char *fnam, int flag);

/*
 * Convert one hexadecimal digit to its numeric value. Returns -1 for any
 * non-hexadecimal character.
 */
static int32 ihex_hex_value(char ch)
{
    if ((ch >= '0') && (ch <= '9'))
        return ch - '0';
    if ((ch >= 'A') && (ch <= 'F'))
        return ch - 'A' + 10;
    if ((ch >= 'a') && (ch <= 'f'))
        return ch - 'a' + 10;
    return -1;
}

/*
 * Parse exactly two Intel HEX digits from *p. The input pointer advances only
 * after a complete byte field is present.
 */
static bool parse_ihex_byte(const char **p, uint8 *byte)
{
    int32 hi, lo;

    if (((*p)[0] == '\0') || ((*p)[1] == '\0'))
        return false;
    hi = ihex_hex_value((*p)[0]);
    lo = ihex_hex_value((*p)[1]);
    if ((hi < 0) || (lo < 0))
        return false;
    *byte = (uint8)((hi << 4) | lo);
    *p += 2;
    return true;
}

/*
 * Parse exactly four Intel HEX digits from *p as a 16-bit address field.
 */
static bool parse_ihex_word(const char **p, int32 *word)
{
    uint8 hi, lo;

    if (!parse_ihex_byte(p, &hi) || !parse_ihex_byte(p, &lo))
        return false;
    *word = ((int32)hi << 8) | lo;
    return true;
}

/*
 * Parse one 16-bit hexadecimal address argument. Addresses are one to four
 * hex digits and are separated from later arguments by whitespace.
 */
static bool parse_i8080_address_arg(const char **p, int32 *addr)
{
    int32 value = 0;
    int32 digits = 0;
    int32 hex;

    while (isspace((unsigned char)**p))
        (*p)++;
    while ((hex = ihex_hex_value(**p)) >= 0) {
        if (digits >= 4)
            return false;
        value = (value << 4) | hex;
        digits++;
        (*p)++;
    }
    if (digits == 0)
        return false;
    *addr = value;
    return true;
}

/*
 * Parse the optional LOAD/DUMP address arguments. A binary load accepts zero
 * or one address; a dump requires two. The caller enforces command-specific
 * arity after this function rejects malformed or extra tokens.
 */
static t_stat parse_i8080_load_range(const char *cptr, int32 *argc,
                                     int32 *start, int32 *end)
{
    const char *p = (cptr == NULL) ? "" : cptr;
    int32 values[2] = {0, 0};
    int32 count = 0;

    while (isspace((unsigned char)*p))
        p++;
    while (*p != '\0') {
        if (count >= 2)
            return SCPE_ARG;
        if (!parse_i8080_address_arg(&p, &values[count]))
            return SCPE_ARG;
        count++;
        while (isspace((unsigned char)*p))
            p++;
        if ((*p != '\0') && (ihex_hex_value(*p) < 0))
            return SCPE_ARG;
    }

    *argc = count;
    if (count > 0)
        *start = values[0];
    if (count > 1)
        *end = values[1];
    return SCPE_OK;
}

/*
 * Return whether a data record fits in the 16-bit i8080 memory image without
 * wrapping through address zero.
 */
static bool ihex_data_fits_memory(int32 addr, int32 cnt)
{
    if (cnt == 0)
        return true;
    return addr <= (ADDRMASK - cnt + 1);
}

/*
 * Parse one Intel HEX record and verify its checksum. Data bytes are returned
 * only after the whole record has been validated.
 */
static t_stat parse_ihex_record(const char *buf, int32 *cnt, int32 *addr,
                                int32 *rtype, uint8 data[IHEX_MAX_DATA])
{
    const char *p;
    uint8 parsed_cnt, parsed_type, parsed_byte;
    int32 parsed_addr;
    uint32 chk;

    if (buf[0] != ':')
        return SCPE_ARG;
    p = buf + 1;
    if (!parse_ihex_byte(&p, &parsed_cnt) ||
        !parse_ihex_word(&p, &parsed_addr) ||
        !parse_ihex_byte(&p, &parsed_type))
        return SCPE_ARG;
    if (parsed_addr > ADDRMASK)
        return SCPE_ARG;

    chk = parsed_cnt + ((parsed_addr >> 8) & BYTEMASK) +
          (parsed_addr & BYTEMASK) + parsed_type;
    for (int32 i = 0; i < (int32)parsed_cnt; i++) {
        if (!parse_ihex_byte(&p, &parsed_byte))
            return SCPE_ARG;
        data[i] = (uint8)parsed_byte;
        chk += parsed_byte;
    }
    if (!parse_ihex_byte(&p, &parsed_byte))
        return SCPE_ARG;
    chk += parsed_byte;
    while (isspace((unsigned char)*p))
        p++;
    if (*p != '\0')
        return SCPE_ARG;
    if ((chk & BYTEMASK) != 0)
        return SCPE_CSUM;

    *cnt = (int32)parsed_cnt;
    *addr = (int32)parsed_addr;
    *rtype = (int32)parsed_type;
    return SCPE_OK;
}

/*
 * Compute the Intel HEX two's-complement checksum for a data record.
 */
static uint8 ihex_checksum(int32 cnt, int32 addr, int32 rtype,
                           const uint8 *data)
{
    uint32 chk = (uint32)cnt + ((uint32)addr >> 8) + ((uint32)addr & BYTEMASK) +
                 (uint32)rtype;

    for (int32 i = 0; i < cnt; i++)
        chk += data[i];
    return (uint8)(-chk);
}

/*
 * Load or dump i8080 memory through the simulator LOAD and DUMP commands.
 * The -H switch selects Intel HEX; otherwise the stream is raw binary.
 */
t_stat sim_load(FILE *fileref, const char *cptr, const char *fnam, int flag)
{
    /* Generic loader signature. This implementation does not use fnam. */
    (void)fnam;

    int32 i, addr = 0, addr0 = 0, cnt = 0, start = 0x10000;
    int32 addr1 = 0, end = 0, rtype;
    char buf[600];
    uint8 data[IHEX_MAX_DATA];
    bool have_addr = false;
    bool have_eof = false;
    t_stat r;

    start = saved_PC & ADDRMASK;
    r = parse_i8080_load_range(cptr, &cnt, &start, &end);
    if (r != SCPE_OK)
        return r;
    addr = start;
    if (flag == 0) {                      // load
        if (sim_switches & SWMASK('H')) { // hex
            if (cnt > 1)                  // 2 arguments - error
                return SCPE_ARG;
            cnt = 0;
            while (fgets(buf, sizeof(buf) - 1, fileref)) {
                r = parse_ihex_record(buf, &i, &addr, &rtype, data);
                if (r != SCPE_OK)
                    return r;
                if (rtype == IHEX_DATA) {
                    if (!ihex_data_fits_memory(addr, i))
                        return SCPE_ARG;
                    if (!have_addr) {
                        addr1 = addr;
                        have_addr = true;
                    }
                    for (int32 j = 0; j < i; j++)
                        put_mbyte((uint16)(addr + j), data[j]);
                    cnt += i;
                    printf("+");
                } else if (rtype == IHEX_EOF) {
                    have_eof = true;
                    break;
                } else {
                    return SCPE_ARG;
                }
            }
            if (!have_eof)
                return SCPE_ARG;
        } else { // binary
            cnt = 0;
            addr1 = addr;
            while ((i = getc(fileref)) != EOF) {
                put_mbyte(addr, i);
                addr++;
                cnt++;
            }
        }
        printf("%d Bytes loaded at %04X\n", cnt, addr1);
        return SCPE_OK;
    } else {          // dump
        if (cnt != 2) // must be 2 arguments
            return SCPE_ARG;
        cnt = 0;
        addr0 = addr;
        if (sim_switches & SWMASK('H')) { // hex
            while (addr <= end) {
                int32 rec_len = end - addr + 1;

                if (rec_len > HLEN)
                    rec_len = HLEN;
                fprintf(fileref, ":%02X%04X00", rec_len, addr);
                for (i = 0; i < rec_len; i++) {
                    data[i] = get_mbyte((uint16)(addr + i));
                    fprintf(fileref, "%02X", data[i]);
                    cnt++;
                }
                fprintf(fileref, "%02X\n",
                        ihex_checksum(rec_len, addr, IHEX_DATA, data));
                addr += rec_len;
            }
            fprintf(fileref, ":00000001FF\n"); // EOF record
        } else {                               // binary
            while (addr <= end) {
                i = get_mbyte(addr);
                putc(i, fileref);
                addr++;
                cnt++;
            }
        }
        printf("%d Bytes dumped from %04X\n", cnt, addr0);
    }
    return SCPE_OK;
}
