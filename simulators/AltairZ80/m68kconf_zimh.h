// AltairZ80 Musashi configuration.
//
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef M68KCONF_ZIMH_H_
#define M68KCONF_ZIMH_H_ 1

#include "m68ksim.h"

#define M68K_OPT_OFF 0
#define M68K_OPT_ON 1
#define M68K_OPT_SPECIFY_HANDLER 2

#define M68K_COMPILE_FOR_MAME M68K_OPT_OFF

#define M68K_EMULATE_010 M68K_OPT_ON
#define M68K_EMULATE_EC020 M68K_OPT_ON
#define M68K_EMULATE_020 M68K_OPT_ON
#define M68K_EMULATE_030 M68K_OPT_ON
#define M68K_EMULATE_040 M68K_OPT_ON

#define M68K_SEPARATE_READS M68K_OPT_OFF
#define M68K_SIMULATE_PD_WRITES M68K_OPT_OFF

#define M68K_EMULATE_INT_ACK M68K_OPT_SPECIFY_HANDLER
#define M68K_INT_ACK_CALLBACK(A) m68k_cpu_irq_ack(A)

#define M68K_EMULATE_BKPT_ACK M68K_OPT_OFF
#define M68K_EMULATE_TRACE M68K_OPT_OFF

#define M68K_EMULATE_RESET M68K_OPT_SPECIFY_HANDLER
#define M68K_RESET_CALLBACK() m68k_cpu_pulse_reset()

#define M68K_CMPILD_HAS_CALLBACK M68K_OPT_OFF
#define M68K_RTE_HAS_CALLBACK M68K_OPT_OFF
#define M68K_TAS_HAS_CALLBACK M68K_OPT_OFF
#define M68K_ILLG_HAS_CALLBACK M68K_OPT_OFF
#define M68K_TRAP_HAS_CALLBACK M68K_OPT_OFF

#define M68K_EMULATE_FC M68K_OPT_SPECIFY_HANDLER
#define M68K_SET_FC_CALLBACK(A) m68k_cpu_set_fc(A)

#define M68K_MONITOR_PC M68K_OPT_OFF
#define M68K_INSTRUCTION_HOOK M68K_OPT_OFF

#define M68K_EMULATE_PREFETCH M68K_OPT_OFF
#define M68K_EMULATE_ADDRESS_ERROR M68K_OPT_OFF

#define M68K_LOG_ENABLE M68K_OPT_OFF
#define M68K_LOG_1010_1111 M68K_OPT_OFF
#define M68K_LOG_TRAP M68K_OPT_OFF

#define M68K_EMULATE_PMMU M68K_OPT_ON
#define M68K_USE_64_BIT M68K_OPT_ON

#define m68k_read_memory_8(A) m68k_cpu_read_byte(A)
#define m68k_read_memory_16(A) m68k_cpu_read_word(A)
#define m68k_read_memory_32(A) m68k_cpu_read_long(A)

#define m68k_write_memory_8(A, V) m68k_cpu_write_byte(A, V)
#define m68k_write_memory_16(A, V) m68k_cpu_write_word(A, V)
#define m68k_write_memory_32(A, V) m68k_cpu_write_long(A, V)

#endif /* M68KCONF_ZIMH_H_ */
