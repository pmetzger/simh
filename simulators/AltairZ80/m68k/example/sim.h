#ifndef SIM__HEADER
#define SIM__HEADER

#include "sim_types.h"

uint_t cpu_read_byte(uint_t address);
uint_t cpu_read_word(uint_t address);
uint_t cpu_read_long(uint_t address);
void cpu_write_byte(uint_t address, uint_t value);
void cpu_write_word(uint_t address, uint_t value);
void cpu_write_long(uint_t address, uint_t value);
void cpu_pulse_reset(void);
void cpu_set_fc(uint_t fc);
int  cpu_irq_ack(int level);
void cpu_instr_callback(int pc);

#endif /* SIM__HEADER */
