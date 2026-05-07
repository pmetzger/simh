/* i8080_symbol.c: Intel 8080/8085 symbolic input and output */
// SPDX-FileCopyrightText: 1997-2005 Charles E. Owen
// SPDX-FileCopyrightText: 2011 William A. Beech
// SPDX-License-Identifier: X11

#include "i8080_symbol_internal.h"

/*
 * The operand syntax records parser grammar separately from the text emitted
 * by the disassembler.
 */
enum i8080_symbol_operand {
    I8080_SYMBOL_INVALID,
    I8080_SYMBOL_NONE,
    I8080_SYMBOL_BYTE_SPACE,
    I8080_SYMBOL_BYTE_COMMA,
    I8080_SYMBOL_WORD_SPACE,
    I8080_SYMBOL_WORD_COMMA,
};

struct i8080_symbol_entry {
    const char *mnemonic;
    enum i8080_symbol_operand operand;
};

#define OP_NONE(text) {text, I8080_SYMBOL_NONE}
#define OP_BYTE_SPACE(text) {text, I8080_SYMBOL_BYTE_SPACE}
#define OP_BYTE_COMMA(text) {text, I8080_SYMBOL_BYTE_COMMA}
#define OP_WORD_SPACE(text) {text, I8080_SYMBOL_WORD_SPACE}
#define OP_WORD_COMMA(text) {text, I8080_SYMBOL_WORD_COMMA}

static const struct i8080_symbol_entry opcodes[256] = {
    [0x00] = OP_NONE("NOP"),
    [0x01] = OP_WORD_COMMA("LXI B"),
    [0x02] = OP_NONE("STAX B"),
    [0x03] = OP_NONE("INX B"),
    [0x04] = OP_NONE("INR B"),
    [0x05] = OP_NONE("DCR B"),
    [0x06] = OP_BYTE_COMMA("MVI B"),
    [0x07] = OP_NONE("RLC"),
    [0x09] = OP_NONE("DAD B"),
    [0x0A] = OP_NONE("LDAX B"),
    [0x0B] = OP_NONE("DCX B"),
    [0x0C] = OP_NONE("INR C"),
    [0x0D] = OP_NONE("DCR C"),
    [0x0E] = OP_BYTE_COMMA("MVI C"),
    [0x0F] = OP_NONE("RRC"),
    [0x11] = OP_WORD_COMMA("LXI D"),
    [0x12] = OP_NONE("STAX D"),
    [0x13] = OP_NONE("INX D"),
    [0x14] = OP_NONE("INR D"),
    [0x15] = OP_NONE("DCR D"),
    [0x16] = OP_BYTE_COMMA("MVI D"),
    [0x17] = OP_NONE("RAL"),
    [0x19] = OP_NONE("DAD D"),
    [0x1A] = OP_NONE("LDAX D"),
    [0x1B] = OP_NONE("DCX D"),
    [0x1C] = OP_NONE("INR E"),
    [0x1D] = OP_NONE("DCR E"),
    [0x1E] = OP_BYTE_COMMA("MVI E"),
    [0x1F] = OP_NONE("RAR"),
    [0x20] = OP_NONE("RIM"),
    [0x21] = OP_WORD_COMMA("LXI H"),
    [0x22] = OP_WORD_SPACE("SHLD"),
    [0x23] = OP_NONE("INX H"),
    [0x24] = OP_NONE("INR H"),
    [0x25] = OP_NONE("DCR H"),
    [0x26] = OP_BYTE_COMMA("MVI H"),
    [0x27] = OP_NONE("DAA"),
    [0x29] = OP_NONE("DAD H"),
    [0x2A] = OP_WORD_SPACE("LHLD"),
    [0x2B] = OP_NONE("DCX H"),
    [0x2C] = OP_NONE("INR L"),
    [0x2D] = OP_NONE("DCR L"),
    [0x2E] = OP_BYTE_COMMA("MVI L"),
    [0x2F] = OP_NONE("CMA"),
    [0x30] = OP_NONE("SIM"),
    [0x31] = OP_WORD_COMMA("LXI SP"),
    [0x32] = OP_WORD_SPACE("STA"),
    [0x33] = OP_NONE("INX SP"),
    [0x34] = OP_NONE("INR M"),
    [0x35] = OP_NONE("DCR M"),
    [0x36] = OP_BYTE_COMMA("MVI M"),
    [0x37] = OP_NONE("STC"),
    [0x39] = OP_NONE("DAD SP"),
    [0x3A] = OP_WORD_SPACE("LDA"),
    [0x3B] = OP_NONE("DCX SP"),
    [0x3C] = OP_NONE("INR A"),
    [0x3D] = OP_NONE("DCR A"),
    [0x3E] = OP_BYTE_COMMA("MVI A"),
    [0x3F] = OP_NONE("CMC"),
    [0x40] = OP_NONE("MOV B,B"),
    [0x41] = OP_NONE("MOV B,C"),
    [0x42] = OP_NONE("MOV B,D"),
    [0x43] = OP_NONE("MOV B,E"),
    [0x44] = OP_NONE("MOV B,H"),
    [0x45] = OP_NONE("MOV B,L"),
    [0x46] = OP_NONE("MOV B,M"),
    [0x47] = OP_NONE("MOV B,A"),
    [0x48] = OP_NONE("MOV C,B"),
    [0x49] = OP_NONE("MOV C,C"),
    [0x4A] = OP_NONE("MOV C,D"),
    [0x4B] = OP_NONE("MOV C,E"),
    [0x4C] = OP_NONE("MOV C,H"),
    [0x4D] = OP_NONE("MOV C,L"),
    [0x4E] = OP_NONE("MOV C,M"),
    [0x4F] = OP_NONE("MOV C,A"),
    [0x50] = OP_NONE("MOV D,B"),
    [0x51] = OP_NONE("MOV D,C"),
    [0x52] = OP_NONE("MOV D,D"),
    [0x53] = OP_NONE("MOV D,E"),
    [0x54] = OP_NONE("MOV D,H"),
    [0x55] = OP_NONE("MOV D,L"),
    [0x56] = OP_NONE("MOV D,M"),
    [0x57] = OP_NONE("MOV D,A"),
    [0x58] = OP_NONE("MOV E,B"),
    [0x59] = OP_NONE("MOV E,C"),
    [0x5A] = OP_NONE("MOV E,D"),
    [0x5B] = OP_NONE("MOV E,E"),
    [0x5C] = OP_NONE("MOV E,H"),
    [0x5D] = OP_NONE("MOV E,L"),
    [0x5E] = OP_NONE("MOV E,M"),
    [0x5F] = OP_NONE("MOV E,A"),
    [0x60] = OP_NONE("MOV H,B"),
    [0x61] = OP_NONE("MOV H,C"),
    [0x62] = OP_NONE("MOV H,D"),
    [0x63] = OP_NONE("MOV H,E"),
    [0x64] = OP_NONE("MOV H,H"),
    [0x65] = OP_NONE("MOV H,L"),
    [0x66] = OP_NONE("MOV H,M"),
    [0x67] = OP_NONE("MOV H,A"),
    [0x68] = OP_NONE("MOV L,B"),
    [0x69] = OP_NONE("MOV L,C"),
    [0x6A] = OP_NONE("MOV L,D"),
    [0x6B] = OP_NONE("MOV L,E"),
    [0x6C] = OP_NONE("MOV L,H"),
    [0x6D] = OP_NONE("MOV L,L"),
    [0x6E] = OP_NONE("MOV L,M"),
    [0x6F] = OP_NONE("MOV L,A"),
    [0x70] = OP_NONE("MOV M,B"),
    [0x71] = OP_NONE("MOV M,C"),
    [0x72] = OP_NONE("MOV M,D"),
    [0x73] = OP_NONE("MOV M,E"),
    [0x74] = OP_NONE("MOV M,H"),
    [0x75] = OP_NONE("MOV M,L"),
    [0x76] = OP_NONE("HLT"),
    [0x77] = OP_NONE("MOV M,A"),
    [0x78] = OP_NONE("MOV A,B"),
    [0x79] = OP_NONE("MOV A,C"),
    [0x7A] = OP_NONE("MOV A,D"),
    [0x7B] = OP_NONE("MOV A,E"),
    [0x7C] = OP_NONE("MOV A,H"),
    [0x7D] = OP_NONE("MOV A,L"),
    [0x7E] = OP_NONE("MOV A,M"),
    [0x7F] = OP_NONE("MOV A,A"),
    [0x80] = OP_NONE("ADD B"),
    [0x81] = OP_NONE("ADD C"),
    [0x82] = OP_NONE("ADD D"),
    [0x83] = OP_NONE("ADD E"),
    [0x84] = OP_NONE("ADD H"),
    [0x85] = OP_NONE("ADD L"),
    [0x86] = OP_NONE("ADD M"),
    [0x87] = OP_NONE("ADD A"),
    [0x88] = OP_NONE("ADC B"),
    [0x89] = OP_NONE("ADC C"),
    [0x8A] = OP_NONE("ADC D"),
    [0x8B] = OP_NONE("ADC E"),
    [0x8C] = OP_NONE("ADC H"),
    [0x8D] = OP_NONE("ADC L"),
    [0x8E] = OP_NONE("ADC M"),
    [0x8F] = OP_NONE("ADC A"),
    [0x90] = OP_NONE("SUB B"),
    [0x91] = OP_NONE("SUB C"),
    [0x92] = OP_NONE("SUB D"),
    [0x93] = OP_NONE("SUB E"),
    [0x94] = OP_NONE("SUB H"),
    [0x95] = OP_NONE("SUB L"),
    [0x96] = OP_NONE("SUB M"),
    [0x97] = OP_NONE("SUB A"),
    [0x98] = OP_NONE("SBB B"),
    [0x99] = OP_NONE("SBB C"),
    [0x9A] = OP_NONE("SBB D"),
    [0x9B] = OP_NONE("SBB E"),
    [0x9C] = OP_NONE("SBB H"),
    [0x9D] = OP_NONE("SBB L"),
    [0x9E] = OP_NONE("SBB M"),
    [0x9F] = OP_NONE("SBB A"),
    [0xA0] = OP_NONE("ANA B"),
    [0xA1] = OP_NONE("ANA C"),
    [0xA2] = OP_NONE("ANA D"),
    [0xA3] = OP_NONE("ANA E"),
    [0xA4] = OP_NONE("ANA H"),
    [0xA5] = OP_NONE("ANA L"),
    [0xA6] = OP_NONE("ANA M"),
    [0xA7] = OP_NONE("ANA A"),
    [0xA8] = OP_NONE("XRA B"),
    [0xA9] = OP_NONE("XRA C"),
    [0xAA] = OP_NONE("XRA D"),
    [0xAB] = OP_NONE("XRA E"),
    [0xAC] = OP_NONE("XRA H"),
    [0xAD] = OP_NONE("XRA L"),
    [0xAE] = OP_NONE("XRA M"),
    [0xAF] = OP_NONE("XRA A"),
    [0xB0] = OP_NONE("ORA B"),
    [0xB1] = OP_NONE("ORA C"),
    [0xB2] = OP_NONE("ORA D"),
    [0xB3] = OP_NONE("ORA E"),
    [0xB4] = OP_NONE("ORA H"),
    [0xB5] = OP_NONE("ORA L"),
    [0xB6] = OP_NONE("ORA M"),
    [0xB7] = OP_NONE("ORA A"),
    [0xB8] = OP_NONE("CMP B"),
    [0xB9] = OP_NONE("CMP C"),
    [0xBA] = OP_NONE("CMP D"),
    [0xBB] = OP_NONE("CMP E"),
    [0xBC] = OP_NONE("CMP H"),
    [0xBD] = OP_NONE("CMP L"),
    [0xBE] = OP_NONE("CMP M"),
    [0xBF] = OP_NONE("CMP A"),
    [0xC0] = OP_NONE("RNZ"),
    [0xC1] = OP_NONE("POP B"),
    [0xC2] = OP_WORD_SPACE("JNZ"),
    [0xC3] = OP_WORD_SPACE("JMP"),
    [0xC4] = OP_WORD_SPACE("CNZ"),
    [0xC5] = OP_NONE("PUSH B"),
    [0xC6] = OP_BYTE_SPACE("ADI"),
    [0xC7] = OP_NONE("RST 0"),
    [0xC8] = OP_NONE("RZ"),
    [0xC9] = OP_NONE("RET"),
    [0xCA] = OP_WORD_SPACE("JZ"),
    [0xCC] = OP_WORD_SPACE("CZ"),
    [0xCD] = OP_WORD_SPACE("CALL"),
    [0xCE] = OP_BYTE_SPACE("ACI"),
    [0xCF] = OP_NONE("RST 1"),
    [0xD0] = OP_NONE("RNC"),
    [0xD1] = OP_NONE("POP D"),
    [0xD2] = OP_WORD_SPACE("JNC"),
    [0xD3] = OP_BYTE_SPACE("OUT"),
    [0xD4] = OP_WORD_SPACE("CNC"),
    [0xD5] = OP_NONE("PUSH D"),
    [0xD6] = OP_BYTE_SPACE("SUI"),
    [0xD7] = OP_NONE("RST 2"),
    [0xD8] = OP_NONE("RC"),
    [0xDA] = OP_WORD_SPACE("JC"),
    [0xDB] = OP_BYTE_SPACE("IN"),
    [0xDC] = OP_WORD_SPACE("CC"),
    [0xDE] = OP_BYTE_SPACE("SBI"),
    [0xDF] = OP_NONE("RST 3"),
    [0xE0] = OP_NONE("RPO"),
    [0xE1] = OP_NONE("POP H"),
    [0xE2] = OP_WORD_SPACE("JPO"),
    [0xE3] = OP_NONE("XTHL"),
    [0xE4] = OP_WORD_SPACE("CPO"),
    [0xE5] = OP_NONE("PUSH H"),
    [0xE6] = OP_BYTE_SPACE("ANI"),
    [0xE7] = OP_NONE("RST 4"),
    [0xE8] = OP_NONE("RPE"),
    [0xE9] = OP_NONE("PCHL"),
    [0xEA] = OP_WORD_SPACE("JPE"),
    [0xEB] = OP_NONE("XCHG"),
    [0xEC] = OP_WORD_SPACE("CPE"),
    [0xEE] = OP_BYTE_SPACE("XRI"),
    [0xEF] = OP_NONE("RST 5"),
    [0xF0] = OP_NONE("RP"),
    [0xF1] = OP_NONE("POP PSW"),
    [0xF2] = OP_WORD_SPACE("JP"),
    [0xF3] = OP_NONE("DI"),
    [0xF4] = OP_WORD_SPACE("CP"),
    [0xF5] = OP_NONE("PUSH PSW"),
    [0xF6] = OP_BYTE_SPACE("ORI"),
    [0xF7] = OP_NONE("RST 6"),
    [0xF8] = OP_NONE("RM"),
    [0xF9] = OP_NONE("SPHL"),
    [0xFA] = OP_WORD_SPACE("JM"),
    [0xFB] = OP_NONE("EI"),
    [0xFC] = OP_WORD_SPACE("CM"),
    [0xFE] = OP_BYTE_SPACE("CPI"),
    [0xFF] = OP_NONE("RST 7"),
};

#undef OP_NONE
#undef OP_BYTE_SPACE
#undef OP_BYTE_COMMA
#undef OP_WORD_SPACE
#undef OP_WORD_COMMA

/*
 * Return the encoded instruction length for an Intel 8080 opcode.
 */
int32 i8080_symbol_instruction_length(uint8 opcode)
{
    switch (opcodes[opcode].operand) {
    case I8080_SYMBOL_NONE:
        return 1;
    case I8080_SYMBOL_BYTE_SPACE:
    case I8080_SYMBOL_BYTE_COMMA:
        return 2;
    case I8080_SYMBOL_WORD_SPACE:
    case I8080_SYMBOL_WORD_COMMA:
        return 3;
    case I8080_SYMBOL_INVALID:
    default:
        return 0;
    }
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
static t_bool i8080_symbol_text_matches(const char *input, const char *symbol,
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

    *remaining = input;
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
 * Return the display mnemonic for one opcode, using the legacy invalid opcode
 * spelling for holes in the i8080 table.
 */
static const char *i8080_symbol_display_text(int opcode)
{
    if (opcodes[opcode].operand == I8080_SYMBOL_INVALID)
        return "???";
    return opcodes[opcode].mnemonic;
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
    const struct i8080_symbol_entry *entry;

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
    entry = &opcodes[inst];
    fprintf(of, "%s", i8080_symbol_display_text(inst));
    if (entry->operand == I8080_SYMBOL_BYTE_COMMA ||
        entry->operand == I8080_SYMBOL_WORD_COMMA)
        fprintf(of, ",");
    if (entry->operand == I8080_SYMBOL_BYTE_SPACE ||
        entry->operand == I8080_SYMBOL_WORD_SPACE)
        fprintf(of, " ");
    if (entry->operand == I8080_SYMBOL_BYTE_COMMA ||
        entry->operand == I8080_SYMBOL_BYTE_SPACE) {
        fprintf(of, "%02X", val[1]);
    }
    if (entry->operand == I8080_SYMBOL_WORD_COMMA ||
        entry->operand == I8080_SYMBOL_WORD_SPACE) {
        adr = val[1] & 0xFF;
        adr |= (val[2] << 8) & 0xff00;
        fprintf(of, "%04X", adr);
    }
    return -(i8080_symbol_instruction_length((uint8)inst) - 1);
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
        const struct i8080_symbol_entry *entry = &opcodes[op];
        uint32 parsed;

        if (entry->operand == I8080_SYMBOL_INVALID)
            continue;
        if (!i8080_symbol_text_matches(cptr, entry->mnemonic, &operand))
            continue;

        if (entry->operand == I8080_SYMBOL_NONE) {
            if (*skip_spaces(operand) != '\0')
                return SCPE_ARG;
            val[0] = op;
            return SCPE_OK;
        }

        if (entry->operand == I8080_SYMBOL_BYTE_COMMA ||
            entry->operand == I8080_SYMBOL_WORD_COMMA) {
            operand = skip_spaces(operand);
            if (*operand != ',')
                continue;
            operand++;
        } else if (!isspace((unsigned char)*operand)) {
            continue;
        }

        if (!i8080_parse_hex_operand(
                operand,
                i8080_symbol_instruction_length((uint8)op) == 2 ? 2 : 4,
                &parsed))
            return SCPE_ARG;

        val[0] = op;
        val[1] = parsed & 0xFF;
        if (i8080_symbol_instruction_length((uint8)op) == 2)
            return -1;
        val[2] = (parsed >> 8) & 0xFF;
        return -2;
    }

    return SCPE_ARG;
}
