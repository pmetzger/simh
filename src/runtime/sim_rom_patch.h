// SPDX-FileCopyrightText: 2026 The ZIMH Contributors
// SPDX-License-Identifier: MIT

#ifndef SIM_ROM_PATCH_H_
#define SIM_ROM_PATCH_H_ 0

#include <stddef.h>
#include <stdint.h>

#include "sim_defs.h"

typedef t_stat (*sim_rom_patch_read_byte)(void *context, t_addr address,
                                          uint8_t *value);
typedef t_stat (*sim_rom_patch_write_byte)(void *context, t_addr address,
                                           uint8_t value);

typedef struct sim_rom_patch {
    const char *name;
    const char *description;
    t_addr address;
    const uint8_t *expected;
    const uint8_t *replacement;
    size_t size;
} sim_rom_patch;

/*
 * Apply a ROM patch after verifying that the target bytes match either the
 * expected unpatched contents or the replacement contents.
 */
t_stat sim_rom_patch_apply(const sim_rom_patch *patch,
                           sim_rom_patch_read_byte read_byte,
                           sim_rom_patch_write_byte write_byte, void *context);

/*
 * Revert a ROM patch after verifying that the target bytes match either the
 * expected unpatched contents or the replacement contents.
 */
t_stat sim_rom_patch_revert(const sim_rom_patch *patch,
                            sim_rom_patch_read_byte read_byte,
                            sim_rom_patch_write_byte write_byte, void *context);

#endif
