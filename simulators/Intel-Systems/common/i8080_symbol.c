/* i8080_symbol.c: Intel 8080/8085 symbolic input and output */
// SPDX-FileCopyrightText: 1997-2005 Charles E. Owen
// SPDX-FileCopyrightText: 2011 William A. Beech
// SPDX-License-Identifier: X11

#include "i8080_symbol_internal.h"

static const char *opcode[] = {
    "NOP",     "LXI B,",   "STAX B",  "INX B", /* 0x00 */
    "INR B",   "DCR B",    "MVI B,",  "RLC",     "???",     "DAD B",
    "LDAX B",  "DCX B",    "INR C",   "DCR C",   "MVI C,",  "RRC",
    "???",     "LXI D,",   "STAX D",  "INX D", /* 0x10 */
    "INR D",   "DCR D",    "MVI D,",  "RAL",     "???",     "DAD D",
    "LDAX D",  "DCX D",    "INR E",   "DCR E",   "MVI E,",  "RAR",
    "RIM",     "LXI H,",   "SHLD ",   "INX H", /* 0x20 */
    "INR H",   "DCR H",    "MVI H,",  "DAA",     "???",     "DAD H",
    "LHLD ",   "DCX H",    "INR L",   "DCR L",   "MVI L,",  "CMA",
    "SIM",     "LXI SP,",  "STA ",    "INX SP", /* 0x30 */
    "INR M",   "DCR M",    "MVI M,",  "STC",     "???",     "DAD SP",
    "LDA ",    "DCX SP",   "INR A",   "DCR A",   "MVI A,",  "CMC",
    "MOV B,B", "MOV B,C",  "MOV B,D", "MOV B,E", /* 0x40 */
    "MOV B,H", "MOV B,L",  "MOV B,M", "MOV B,A", "MOV C,B", "MOV C,C",
    "MOV C,D", "MOV C,E",  "MOV C,H", "MOV C,L", "MOV C,M", "MOV C,A",
    "MOV D,B", "MOV D,C",  "MOV D,D", "MOV D,E", /* 0x50 */
    "MOV D,H", "MOV D,L",  "MOV D,M", "MOV D,A", "MOV E,B", "MOV E,C",
    "MOV E,D", "MOV E,E",  "MOV E,H", "MOV E,L", "MOV E,M", "MOV E,A",
    "MOV H,B", "MOV H,C",  "MOV H,D", "MOV H,E", /* 0x60 */
    "MOV H,H", "MOV H,L",  "MOV H,M", "MOV H,A", "MOV L,B", "MOV L,C",
    "MOV L,D", "MOV L,E",  "MOV L,H", "MOV L,L", "MOV L,M", "MOV L,A",
    "MOV M,B", "MOV M,C",  "MOV M,D", "MOV M,E", /* 0x70 */
    "MOV M,H", "MOV M,L",  "HLT",     "MOV M,A", "MOV A,B", "MOV A,C",
    "MOV A,D", "MOV A,E",  "MOV A,H", "MOV A,L", "MOV A,M", "MOV A,A",
    "ADD B",   "ADD C",    "ADD D",   "ADD E", /* 0x80 */
    "ADD H",   "ADD L",    "ADD M",   "ADD A",   "ADC B",   "ADC C",
    "ADC D",   "ADC E",    "ADC H",   "ADC L",   "ADC M",   "ADC A",
    "SUB B",   "SUB C",    "SUB D",   "SUB E", /* 0x90 */
    "SUB H",   "SUB L",    "SUB M",   "SUB A",   "SBB B",   "SBB C",
    "SBB D",   "SBB E",    "SBB H",   "SBB L",   "SBB M",   "SBB A",
    "ANA B",   "ANA C",    "ANA D",   "ANA E", /* 0xA0 */
    "ANA H",   "ANA L",    "ANA M",   "ANA A",   "XRA B",   "XRA C",
    "XRA D",   "XRA E",    "XRA H",   "XRA L",   "XRA M",   "XRA A",
    "ORA B",   "ORA C",    "ORA D",   "ORA E", /* 0xB0 */
    "ORA H",   "ORA L",    "ORA M",   "ORA A",   "CMP B",   "CMP C",
    "CMP D",   "CMP E",    "CMP H",   "CMP L",   "CMP M",   "CMP A",
    "RNZ",     "POP B",    "JNZ ",    "JMP ", /* 0xC0 */
    "CNZ ",    "PUSH B",   "ADI ",    "RST 0",   "RZ",      "RET",
    "JZ ",     "???",      "CZ ",     "CALL ",   "ACI ",    "RST 1",
    "RNC",     "POP D",    "JNC ",    "OUT ", /* 0xD0 */
    "CNC ",    "PUSH D",   "SUI ",    "RST 2",   "RC",      "???",
    "JC ",     "IN ",      "CC ",     "???",     "SBI ",    "RST 3",
    "RPO",     "POP H",    "JPO ",    "XTHL", /* 0xE0 */
    "CPO ",    "PUSH H",   "ANI ",    "RST 4",   "RPE",     "PCHL",
    "JPE ",    "XCHG",     "CPE ",    "???",     "XRI ",    "RST 5",
    "RP",      "POP PSW",  "JP ",     "DI", /* 0xF0 */
    "CP ",     "PUSH PSW", "ORI ",    "RST 6",   "RM",      "SPHL",
    "JM ",     "EI",       "CM ",     "???",     "CPI ",    "RST 7",
};

static const int32 oplen[256] = {
    1, 3, 1, 1, 1, 1, 2, 1, 0, 1, 1, 1, 1, 1, 2, 1, 0, 3, 1, 1, 1, 1, 2, 1,
    0, 1, 1, 1, 1, 1, 2, 1, 1, 3, 3, 1, 1, 1, 2, 1, 0, 1, 3, 1, 1, 1, 2, 1,
    1, 3, 3, 1, 1, 1, 2, 1, 0, 1, 3, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 3, 3, 3, 1, 2, 1, 1, 1, 3, 0, 3, 3, 2, 1, 1, 1, 3, 2, 3, 1, 2, 1,
    1, 0, 3, 2, 3, 0, 2, 1, 1, 1, 3, 1, 3, 1, 2, 1, 1, 1, 3, 1, 3, 0, 2, 1,
    1, 1, 3, 1, 3, 1, 2, 1, 1, 1, 3, 1, 3, 0, 2, 1,
};

/*
 * Return the encoded instruction length for an Intel 8080 opcode.
 */
int32 i8080_symbol_instruction_length(uint8 opcode)
{
    return oplen[opcode];
}

/*
 * Skip host whitespace in symbolic input.
 */
static const char *skip_spaces(const char *text)
{
    while (isspace((unsigned char)*text))
        text++;
    return text;
}

/*
 * Return the numeric value of a hex digit, or -1 for a non-hex byte.
 */
static int hex_digit_value(unsigned char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    ch = (unsigned char)toupper(ch);
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

/*
 * Match an opcode table spelling against user input while allowing
 * case-insensitive input and human spacing around comma delimiters.
 */
static t_bool i8080_symbol_prefix_matches(const char *input, const char *symbol,
                                          const char **remaining)
{
    while (*symbol != '\0') {
        if (isspace((unsigned char)*symbol)) {
            if (!isspace((unsigned char)*input))
                return FALSE;
            input = skip_spaces(input);
            symbol = skip_spaces(symbol);
            continue;
        }

        if (*symbol == ',') {
            input = skip_spaces(input);
            if (*input != ',')
                return FALSE;
            input++;
            input = skip_spaces(input);
            symbol++;
            continue;
        }

        if (toupper((unsigned char)*input) != (unsigned char)*symbol)
            return FALSE;
        input++;
        symbol++;
    }

    *remaining = skip_spaces(input);
    return TRUE;
}

/*
 * Parse a byte or word operand in the same hex notation emitted by
 * fprint_sym().
 */
static t_bool i8080_parse_hex_operand(const char *text, int max_digits,
                                      uint32 *value)
{
    int digit;
    int digits = 0;
    uint32 parsed = 0;

    text = skip_spaces(text);
    while ((digit = hex_digit_value((unsigned char)*text)) >= 0) {
        if (digits == max_digits)
            return FALSE;
        parsed = (parsed << 4) | (uint32)digit;
        digits++;
        text++;
    }

    if (digits == 0)
        return FALSE;

    text = skip_spaces(text);
    if (*text != '\0')
        return FALSE;

    *value = parsed;
    return TRUE;
}

/*
 * Implement SIMH symbolic output for Intel 8080/8085 memory words.
 */
t_stat fprint_sym(FILE *of, t_addr addr, t_value *val, UNIT *uptr, int32 sw)
{
    /* Generic symbolic output signature.
       This implementation does not use every parameter. */
    (void)addr;

    int32 c1, c2, inst, adr;

    (void)uptr;
    c1 = (val[0] >> 8) & 0x7F;
    c2 = val[0] & 0x7F;
    if (sw & SWMASK('A')) {
        fprintf(of, (c2 < 0x20) ? "<%02X>" : "%c", c2);
        return SCPE_OK;
    }
    if (sw & SWMASK('C')) {
        fprintf(of, (c1 < 0x20) ? "<%02X>" : "%c", c1);
        fprintf(of, (c2 < 0x20) ? "<%02X>" : "%c", c2);
        return SCPE_OK;
    }
    if (!(sw & SWMASK('M')))
        return SCPE_ARG;
    inst = val[0];
    fprintf(of, "%s", opcode[inst]);
    if (oplen[inst] == 2) {
        fprintf(of, "%02X", val[1]);
    }
    if (oplen[inst] == 3) {
        adr = val[1] & 0xFF;
        adr |= (val[2] << 8) & 0xff00;
        fprintf(of, "%04X", adr);
    }
    return -(oplen[inst] - 1);
}

/*
 * Implement SIMH symbolic input for Intel 8080/8085 memory words.
 */
t_stat parse_sym(const char *cptr, t_addr addr, UNIT *uptr, t_value *val,
                 int32 sw)
{
    /* Generic symbolic input signature.
       This implementation does not use every parameter. */
    (void)addr;
    (void)uptr;

    size_t op;

    cptr = skip_spaces(cptr);
    if ((sw & SWMASK('A')) || ((*cptr == '\'') && cptr++)) {
        if (cptr[0] == 0)
            return SCPE_ARG;
        val[0] = (unsigned char)cptr[0];
        return SCPE_OK;
    }
    if ((sw & SWMASK('C')) || ((*cptr == '"') && cptr++)) {
        if (cptr[0] == 0)
            return SCPE_ARG;
        val[0] = ((uint32)(unsigned char)cptr[0] << 8) +
                 (uint32)(unsigned char)cptr[1];
        return SCPE_OK;
    }

    for (op = 0; op < 256; op++) {
        const char *operand;
        uint32 parsed;

        if (oplen[op] == 0)
            continue;
        if (!i8080_symbol_prefix_matches(cptr, opcode[op], &operand))
            continue;

        if (oplen[op] == 1) {
            if (*operand != '\0')
                return SCPE_ARG;
            val[0] = op;
            return SCPE_OK;
        }

        if (!i8080_parse_hex_operand(operand, oplen[op] == 2 ? 2 : 4, &parsed))
            return SCPE_ARG;

        val[0] = op;
        val[1] = parsed & 0xFF;
        if (oplen[op] == 2)
            return -1;
        val[2] = (parsed >> 8) & 0xFF;
        return -2;
    }

    return SCPE_ARG;
}
