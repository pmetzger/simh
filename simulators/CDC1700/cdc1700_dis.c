/*

   Copyright (c) 2015-2017, John Forecast

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   JOHN FORECAST BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of John Forecast shall not
   be used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from John Forecast.

*/

/* cdc1700_dis.c: CDC1700 disassembler
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cdc1700_defs.h"

extern uint16_t Areg, Mreg, Preg, Qreg, RelBase;
extern uint16_t LoadFromMem(uint16_t);
extern uint8_t P[];
extern DEVICE cpu_dev;
extern UNIT cpu_unit;
extern t_stat disEffectiveAddr(uint16_t, uint16_t, uint16_t *, uint16_t *);
extern uint16_t doADDinternal(uint16_t, uint16_t);

const char *opName[] = {
  "???", "JMP", "MUI", "DVI", "STQ", "RTJ", "STA", "SPA",
  "ADD", "SUB", "AND", "EOR", "LDA", "RAO", "LDQ", "ADQ"
};

const char *idxName[] = {
  "", ",I", ",Q", ",B"
};

const char *spcName[] = {
  "SLS", "???", "INP", "OUT", "EIN", "IIN", "SPB", "CPB",
  "???", "INA", "ENA", "NOP", "ENQ", "INQ", "EXI", "???"
};

const char *skpName[] = {
  "SAZ", "SAN", "SAP", "SAM", "SQZ", "SQN", "SQP", "SQM",
  "SWS", "SWN", "SOV", "SNO", "SPE", "SNP", "SPF", "SNF"
};

const char *interName[] = {
  "SET", "TRM", "TRQ", "TRB", "TRA", "AAM", "AAQ", "AAB",
  "CLR", "TCM", "TCQ", "TCB", "TCA", "EAM", "EAQ", "EAB",
  "SET", "TRM", "TRQ", "TRB", "TRA", "LAM", "LAQ", "LAB",
  "NOOP", NULL, NULL, NULL, NULL, "CAM", "CAQ", "CAB"
};

const char *destName[] = {
  "", "M", "Q", "Q,M", "A", "A,M", "A,Q", "A,Q,M"
};

const char *shiftName[] = {
  NULL, "QRS", "ARS", "LRS", NULL, "QLS", "ALS", "LLS"
};

/*
 * Enhanced instruction set mnemonics
 */
char enhRegChar[] = {
  ' ', '1', '2', '3', '4', 'Q', 'A', 'I'
};

const char *enhIdxName[] = {
  "", ",1", ",2", ",3", ",4", ",Q", ",A", ",I"
};

char enhSkipType[] = {
  'Z', 'N', 'P', 'M'
};

char enhSkipReg[] = {
  '4', '1', '2', '3'
};

const char *enhMiscName0[] = {
  "???", "LMM", "LRG", "SRG", "SIO", "SPS", "DMI", "CBP",
  "GPE", "GPO", "ASC", "APM", "PM0", "PM1"
};
#define ENH_MAXMISC0    0xD

const char *enhMiscName1[] = {
  "LUB", "LLB", "EMS", "WPR", "RPR", "ECC"
};
#define ENH_MAXMISC1    0x5

const char *enhFldName[] = {
  "???", "???", "SFZ", "SFN", "LFA", "SFA", "CLF", "SEF"
};

#define TARGET_COLUMN   48

/*
 * Generate a single line of text for an instruction. Format is:
 *
 * c xxxx yyyy zzzz     <instr>       <targ>
 *
 * or if enhanced instruction set is enabled
 *
 * c xxxx yyyy zzzz aaaa    <instr>       <targ>
 *
 * where:
 *
 *       |P     Normal/Protected location
 *      xxxx    Memory address of instruction in hex
 *      yyyy    First word of instruction in hex
 *      zzzz    Second word of instruction in hex, replaced by spaces if
 *              not present
 *      aaaa    Third word of instruction in hex, replaced by spaces if
 *              not present
 *      <instr> Disassmbled instruction
 *      <targ>  Optional target address and contents
 *
 * Returns:
 *      # of words consumed by the instruction
 */

int disassem(char *buf, size_t buf_size, uint16_t addr, bool dbg, bool targ,
             bool exec)
{
  int consumed = 1;
  char prot = ISPROTECTED(addr) ? 'P' : ' ';
  char optional[8], optional2[8], temp[8], decoded[64], enhInstr[8];
  const char *mode, *spc, *shift, *inter, *dest;
  uint16_t instr = LoadFromMem(addr);
  uint16_t instr2, enhMode;
  uint16_t delta = instr & OPC_ADDRMASK;
  uint8_t more = 0, isconst = 0, enhRB;
  uint16_t t;
  bool enhValid = false, enhChar = false;

  strlcpy(optional, "    ", sizeof(optional));
  strlcpy(optional2, "    ", sizeof(optional2));
  strlcpy(decoded, "UNDEF", sizeof(decoded));

  if ((instr & OPC_MASK) != 0) {
    if ((instr & MOD_RE) == 0)
      mode = delta == 0 ? "+    " : "-    ";
    else mode = "*    ";

    switch (instr & OPC_MASK) {
      case OPC_ADQ:
      case OPC_LDQ:
      case OPC_LDA:
      case OPC_EOR:
      case OPC_AND:
      case OPC_SUB:
      case OPC_ADD:
      case OPC_DVI:
      case OPC_MUI:
        if (ISCONSTANT(instr))
          isconst = 1;
        break;
    }

    if (delta == 0) {
      consumed++;
      snprintf(optional, sizeof(optional), "%04X", LoadFromMem(addr + 1));
      snprintf(temp, sizeof(temp), "$%04X", LoadFromMem(addr + 1));
    } else snprintf(temp, sizeof(temp), "$%02X", delta);

    snprintf(decoded, sizeof(decoded), "%s%s%s%s%s%s%s",
            opName[(instr & OPC_MASK) >> 12],
            mode,
            isconst ? "=" : "",
            (instr & MOD_IN) != 0 ? "(" : "",
            temp,
            (instr & MOD_IN) != 0 ? ")" : "",
            idxName[(instr & (MOD_I1 | MOD_I2)) >> 8]);
  } else {
    spc = spcName[(instr & OPC_SPECIALMASK) >> 8];

    switch (instr & OPC_SPECIALMASK) {
      case OPC_IIN:
      case OPC_EIN:
      case OPC_SPB:
      case OPC_CPB:
        switch (INSTR_SET) {
          case INSTR_ORIGINAL:
            /*
             * Character addressing enable/disable is only available as
             * an extension to the original set.
             */
            switch (instr) {
              case OPC_ECA:
                snprintf(decoded, sizeof(decoded), "%s", "ECA");
                break;

              case OPC_DCA:
                snprintf(decoded, sizeof(decoded), "%s", "DCA");
                break;

              default:
                snprintf(decoded, sizeof(decoded), "%s", spc);
                break;
            }
            break;

          case INSTR_BASIC:
            if (delta == 0) {
              snprintf(decoded, sizeof(decoded), "%s", spc);
            } else {
              snprintf(decoded, sizeof(decoded), "%s", "NOP  [ Possible enhanced instruction");
            }
            break;

          case INSTR_ENHANCED:
            if (delta != 0) {
              switch (instr & OPC_SPECIALMASK) {
                case OPC_IIN:
                  if (((instr & OPC_FLDF3A) != OPC_FLDRSV1) &&
                      ((instr & OPC_FLDF3A) != OPC_FLDRSV2)) {
                    instr2 = LoadFromMem(addr + 1);
                    delta = instr2 & OPC_ADDRMASK;

                    if (((instr2 & OPC_FLDSTR) - (instr2 & OPC_FLDLTH)) >= 0) {
                      consumed++;

                      if ((instr & MOD_ENHRE) == 0)
                        mode = delta == 0 ? "+    " : "-    ";
                      else mode = "*    ";

                      snprintf(optional, sizeof(optional), "%04X", instr2);
                      if (delta == 0) {
                        consumed++;
                        snprintf(optional2, sizeof(optional2), "%04X", LoadFromMem(addr + 2));
                        snprintf(temp, sizeof(temp), "$%04X", LoadFromMem(addr + 2));
                      } else snprintf(temp, sizeof(temp), "$%02X", delta);

                      snprintf(decoded, sizeof(decoded), "%s%s%s%s%s,%d,%d%s",
                              enhFldName[instr & OPC_FLDF3A],
                              mode,
                              (instr & MOD_ENHIN) != 0 ? "(" : "",
                              temp,
                              (instr & MOD_ENHIN) != 0 ? ")" : "",
                              (instr2 & OPC_FLDSTR) >> 12,
                              ((instr2 & OPC_FLDLTH) >> 8) + 1,
                              enhIdxName[(instr & MOD_ENHRA) >> 3]);
                      break;
                    }
                  }
                  strlcpy(decoded, "UNDEF", sizeof(decoded));
                  targ = false;
                  break;

                case OPC_EIN:
                  instr2 = LoadFromMem(addr + 1);
                  enhMode = (instr2 & OPC_ENHF5) >> 8;
                  enhRB = instr & MOD_ENHRB;

                  switch (instr2 & OPC_ENHF4) {
                    case OPC_STOSJMP:
                      if (enhMode == 0) {
                        enhValid = true;
                        if (enhRB == REG_NOREG)
                          strlcpy(enhInstr, "SJE", sizeof(enhInstr));
                        else snprintf(enhInstr, sizeof(enhInstr), "SJ%c", enhRegChar[enhRB]);
                      }
                      break;

                    case OPC_STOADD:
                      if ((enhMode == 0) && (enhRB != REG_NOREG)) {
                        enhValid = true;
                        snprintf(enhInstr, sizeof(enhInstr), "AR%c", enhRegChar[enhRB]);
                      }
                      break;

                    case OPC_STOSUB:
                      if ((enhMode == 0) && (enhRB != REG_NOREG)) {
                        enhValid = true;
                        snprintf(enhInstr, sizeof(enhInstr), "SB%c", enhRegChar[enhRB]);
                      }
                      break;

                    case OPC_STOAND:
                      if (enhRB != REG_NOREG)
                        switch (enhMode) {
                          case WORD_REG:
                            enhValid = true;
                            snprintf(enhInstr, sizeof(enhInstr), "AN%c", enhRegChar[enhRB]);
                            break;

                          case WORD_MEM:
                            enhValid = true;
                            snprintf(enhInstr, sizeof(enhInstr), "AM%c", enhRegChar[enhRB]);
                            break;
                        }
                      break;

                    case OPC_STOLOADST:
                      switch (enhMode) {
                        case WORD_REG:
                          if (enhRB != REG_NOREG) {
                            enhValid = true;
                            snprintf(enhInstr, sizeof(enhInstr), "LR%c", enhRegChar[enhRB]);
                          }
                          break;

                        case WORD_MEM:
                          if (enhRB != REG_NOREG) {
                            enhValid = true;
                            snprintf(enhInstr, sizeof(enhInstr), "SR%c", enhRegChar[enhRB]);
                          }
                          break;

                        case CHAR_REG:
                          if (enhRB != REG_NOREG) {
                            enhValid = true;
                            enhChar = true;
                            strlcpy(enhInstr, "LCA", sizeof(enhInstr));
                          }
                          break;

                        case CHAR_MEM:
                          if (enhRB != REG_NOREG) {
                            enhValid = true;
                            enhChar = true;
                            strlcpy(enhInstr, "SCA", sizeof(enhInstr));
                          }
                          break;
                      }
                      break;

                    case OPC_STOOR:
                      if (enhRB != REG_NOREG)
                        switch (enhMode) {
                          case WORD_REG:
                            enhValid = true;
                            snprintf(enhInstr, sizeof(enhInstr), "OR%c", enhRegChar[enhRB]);
                            break;

                          case WORD_MEM:
                            enhValid = true;
                            snprintf(enhInstr, sizeof(enhInstr), "OM%c", enhRegChar[enhRB]);
                            break;
                        }
                      break;

                    case OPC_STOCRE:
                      switch (enhMode) {
                        case WORD_REG:
                          if (enhRB != REG_NOREG) {
                            enhValid = true;
                            snprintf(enhInstr, sizeof(enhInstr), "C%cE", enhRegChar[enhRB]);
                          }
                          break;

                        case CHAR_REG:
                          if (enhRB != REG_NOREG) {
                            enhValid = true;
                            enhChar = true;
                            strlcpy(enhInstr, "CCE", sizeof(enhInstr));
                          }
                          break;
                      }
                      break;
                  }
                  if (enhValid) {
                    delta = instr2 & OPC_ADDRMASK;
                    consumed++;

                    if ((instr & MOD_ENHRE) == 0)
                      mode = delta == 0 ? "+    " : "-    ";
                    else mode = "*    ";

                    snprintf(optional, sizeof(optional), "%04X", instr2);
                    if (delta == 0) {
                      consumed++;
                      snprintf(optional2, sizeof(optional2), "%04X", LoadFromMem(addr + 2));
                      snprintf(temp, sizeof(temp), "$%04X", LoadFromMem(addr + 2));
                    } else snprintf(temp, sizeof(temp), "$%02X", delta);

                    if (!enhChar) {
                      if ((delta == 0) &&
                          (instr & (MOD_ENHRE | MOD_ENHIN)) == 0)
                        isconst = 1;

                      snprintf(decoded, sizeof(decoded), "%s%s%s%s%s%s%s",
                              enhInstr,
                              mode,
                              isconst ? "=" : "",
                              (instr & MOD_ENHIN) != 0 ? "(" : "",
                              temp,
                              (instr & MOD_ENHIN) != 0 ? ")" : "",
                              enhIdxName[(instr & MOD_ENHRA) >> 3]);
                    } else {
                      snprintf(decoded, sizeof(decoded), "%s%s%s%s%s%s%s",
                              enhInstr,
                              mode,
                              (instr & MOD_ENHIN) != 0 ? "(" : "",
                              temp,
                              (instr & MOD_ENHIN) != 0 ? ")" : "",
                              enhIdxName[enhRB],
                              enhIdxName[(instr & MOD_ENHRA) >> 3]);
                    }
                  } else {
                    strlcpy(decoded, "UNDEF", sizeof(decoded));
                    targ = false;
                  }
                  break;

                case OPC_SPB:
                  if ((instr & OPC_DRPMBZ) == 0) {
                    char reg = enhRegChar[(instr & OPC_DRPRA) >> 5];
                    uint8_t sk = instr & OPC_DRPSK;

                    snprintf(decoded, sizeof(decoded), "D%cP     $%1X", reg, sk);
                    break;
                  }
                  break;

                case OPC_CPB:
                  if ((instr & OPC_ENHXFRF2A) == 0) {
                    if ((instr & (OPC_ENHXFRRA | OPC_ENHXFRRB)) != 0) {
                      char ra = enhRegChar[(instr & OPC_ENHXFRRA) >> 5];
                      char rb = enhRegChar[instr & OPC_ENHXFRRB];

                      snprintf(decoded, sizeof(decoded), "XF%c     %c", ra, rb);
                      break;
                    }
                  }
                  strlcpy(decoded, "UNDEF", sizeof(decoded));
                  targ = false;
                  break;
              }
            } else {
              snprintf(decoded, sizeof(decoded), "%s", spc);
              targ = false;
            }
            break;
        }
        break;

      case OPC_NOP:
        switch (INSTR_SET) {
          case INSTR_ORIGINAL:
            snprintf(decoded, sizeof(decoded), "%s", spc);
            break;

          case INSTR_BASIC:
            if (delta != 0) {
              snprintf(decoded, sizeof(decoded), "%s", "NOP  [ Possible enhanced instruction");
            } else snprintf(decoded, sizeof(decoded), "%s", spc);
            break;

          case INSTR_ENHANCED:
            if (delta != 0) {
              uint8_t miscFN = instr & OPC_MISCF3;
              char reg = enhRegChar[(instr & OPC_MISCRA) >> 5];

              targ = false;
              if ((instr & OPC_MISCRA) == 0) {
                if (miscFN <= ENH_MAXMISC0)
                  spc = enhMiscName0[miscFN];
                else spc = "UNDEF";
                snprintf(decoded, sizeof(decoded), "%s", spc);
              } else {
                if (miscFN <= ENH_MAXMISC1) {
                  spc = enhMiscName1[miscFN];
                  snprintf(decoded, sizeof(decoded), "%s     %c", spc, reg);
                } else strlcpy(decoded, "UNDEF", sizeof(decoded));
              }
            } else snprintf(decoded, sizeof(decoded), "%s", spc);
            break;
        }
        break;

      case OPC_EXI:
        snprintf(decoded, sizeof(decoded), "%s     $%02X", spc, delta);
        break;

      case OPC_SKIPS:
        snprintf(decoded, sizeof(decoded), "%s     $%01X",
                skpName[(instr & OPC_SKIPMASK) >> 4], instr & OPC_SKIPCOUNT);
        break;

      case OPC_SLS:
        if (delta != 0) {
          switch (INSTR_SET) {
            case INSTR_BASIC:
              snprintf(decoded, sizeof(decoded), "%s", "NOP  [ Possible enhanced instruction");
              break;

            case INSTR_ORIGINAL:
              snprintf(decoded, sizeof(decoded), "%s     $%02X", spc, delta);
              break;

            case INSTR_ENHANCED:
              {
                char reg = enhSkipReg[(instr & OPC_ENHSKIPREG) >> 6];
                char type = enhSkipType[(instr & OPC_ENHSKIPTY) >> 4];
                uint8_t sk = instr & OPC_ENHSKIPCNT;

                snprintf(decoded, sizeof(decoded), "S%c%c     $%1X", reg, type, sk);
              }
              break;
          }
          break;
        }
        strlcpy(decoded, spc, sizeof(decoded));
        break;

      case OPC_INP:
      case OPC_OUT:
      case OPC_INA:
      case OPC_ENA:
      case OPC_ENQ:
      case OPC_INQ:
        snprintf(decoded, sizeof(decoded), "%s     $%02X", spc, delta);
        break;

      case OPC_INTER:
        t = instr & (MOD_LP | MOD_XR | MOD_O_A | MOD_O_Q | MOD_O_M);
        inter = interName[t >> 3];
        dest = destName[instr & (MOD_D_A | MOD_D_Q | MOD_D_M)];
        if (inter != NULL)
          snprintf(decoded, sizeof(decoded), "%s     %s", inter, dest);
        break;

      case OPC_SHIFTS:
        shift = shiftName[(instr & OPC_SHIFTMASK) >> 5];
        if (shift != NULL)
          snprintf(decoded, sizeof(decoded), "%s     $%X", shift, instr & OPC_SHIFTCOUNT);
        break;
    }
  }

  if (dbg) {
    if (INSTR_SET == INSTR_ENHANCED)
      snprintf(buf, buf_size, "%c %04X %04X %s %s         %s",
               prot, addr, instr, optional, optional2, decoded);
    else
      snprintf(buf, buf_size, "%c %04X %04X %s         %s",
               prot, addr, instr, optional, decoded);
  } else {
    if (INSTR_SET == INSTR_ENHANCED)
      snprintf(buf, buf_size, "%c %04X %s %s               %s",
               prot, instr, optional, optional2, decoded);
    else
      snprintf(buf, buf_size, "%c %04X %s               %s",
               prot, instr, optional, decoded);
  }

  if (targ) {
    const char *rel = "";
    bool indJmp = false;
    uint16_t taddr, taddr2,  base;

    switch (instr & OPC_MASK) {
      case OPC_ADQ:
      case OPC_LDQ:
      case OPC_RAO:
      case OPC_LDA:
      case OPC_EOR:
      case OPC_AND:
      case OPC_SUB:
      case OPC_ADD:
      case OPC_SPA:
      case OPC_STA:
      case OPC_STQ:
      case OPC_DVI:
      case OPC_MUI:
        if (((instr & (MOD_IN | MOD_I1 | MOD_I2)) == 0) || exec)
          if (disEffectiveAddr(addr, instr, &base, &taddr) == SCPE_OK) {
            more = cpu_dev.dctrl & DBG_FULL ? 2 : 1;
            taddr2 = taddr;
            if (((instr & (MOD_RE | MOD_IN | MOD_I1 | MOD_I2)) == MOD_RE) &&
                ((sim_switches & SWMASK('R')) != 0)) {
              taddr2 -= RelBase;
              rel = "*";
            }
          }
        break;

      case OPC_JMP:
        if (((instr & (MOD_IN | MOD_I1 | MOD_I2)) == MOD_IN) & !dbg) {
          if (disEffectiveAddr(addr, instr & ~MOD_IN, &base, &taddr) == SCPE_OK) {
            taddr2 = taddr;
            indJmp = true;
            if (((instr & MOD_RE) != 0) && ((sim_switches & SWMASK('R')) != 0)) {
              taddr2 -= RelBase;
              rel = "*";
            }
            break;
          }
        }
        FALLTHROUGH;

      case OPC_RTJ:
        if (((instr & (MOD_IN | MOD_I1 | MOD_I2)) != 0) & !dbg)
          break;

        if (disEffectiveAddr(addr, instr, &base, &taddr) == SCPE_OK) {
          more = cpu_dev.dctrl & DBG_FULL ? 2 : 1;
          taddr2 = taddr;
          if (((instr & (MOD_RE | MOD_IN | MOD_I1 | MOD_I2)) == MOD_RE) &&
              ((sim_switches & SWMASK('R')) != 0)) {
            taddr2 -= RelBase;
            rel = "*";
          }
        }
        break;

      case OPC_SPECIAL:
        switch (instr & OPC_SPECIALMASK) {
          case OPC_SLS:
            break;

          case OPC_SKIPS:
            taddr = doADDinternal(MEMADDR(addr + 1), instr & OPC_SKIPCOUNT);
            taddr2 = taddr;
            if ((sim_switches & SWMASK('R')) != 0)
              taddr2 -= RelBase;
            more = 1;
            break;

          case OPC_SPB:
          case OPC_CPB:
            if (exec) {
              taddr = Qreg;
              taddr2 = taddr;
              if ((sim_switches & SWMASK('R')) != 0)
                taddr2 -= RelBase;
              more = 1;
            }
            break;

          case OPC_INP:
          case OPC_OUT:
            taddr = doADDinternal(addr, EXTEND8(instr & OPC_MODMASK));
            taddr2 = taddr;
            if ((sim_switches & SWMASK('R')) != 0)
              taddr2 -= RelBase;
            more = 1;
            break;

          case OPC_EIN:
          case OPC_IIN:
          case OPC_INTER:
          case OPC_INA:
          case OPC_ENA:
          case OPC_NOP:
          case OPC_ENQ:
          case OPC_INQ:
          case OPC_EXI:
          case OPC_SHIFTS:
            break;
        }
        break;
    }

    if (more || indJmp) {
      size_t len = strnlen(buf, buf_size);

      while ((len < TARGET_COLUMN) && (len + 1 < buf_size)) {
        buf[len++] = ' ';
        buf[len] = '\0';
      }

      if (len < buf_size) {
        char *target = buf + len;
        size_t target_size = buf_size - len;

        if (indJmp) {
          snprintf(target, target_size, "[ => (%04X%s)", taddr2, rel);
        } else {
          switch (more) {
            case 1:
              snprintf(target, target_size, "[ => %04X%s %s {%04X}", taddr2,
                       rel, P[MEMADDR(taddr)] ? "(P)" : "",
                       LoadFromMem(taddr));
              break;

            case 2:
              snprintf(target, target_size, "[ => %04X%s (B:%04X%s) %s {%04X}",
                       taddr2, rel, base, rel, P[MEMADDR(taddr)] ? "(P)" : "",
                       LoadFromMem(taddr));
              break;
          }
        }
      }
    }
  }
  return consumed;
}
