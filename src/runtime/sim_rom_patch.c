// SPDX-FileCopyrightText: 2026 The ZIMH Contributors
// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sim_defs.h"
#include "sim_rom_patch.h"

enum sim_rom_patch_match {
    SIM_ROM_PATCH_MATCH_NO,
    SIM_ROM_PATCH_MATCH_YES,
};

/*
 * Check whether a ROM patch target range already contains a candidate byte
 * sequence.  Callback errors are returned directly so callers can distinguish
 * an inaccessible ROM from a plain byte mismatch.
 */
static t_stat sim_rom_patch_matches(const sim_rom_patch *patch,
                                    sim_rom_patch_read_byte read_byte,
                                    void *context, const uint8_t *candidate,
                                    enum sim_rom_patch_match *match)
{
    *match = SIM_ROM_PATCH_MATCH_YES;

    for (size_t i = 0; i < patch->size; i++) {
        uint8_t actual;
        t_stat r = read_byte(context, patch->address + i, &actual);

        if (r != SCPE_OK)
            return r;
        if (actual != candidate[i])
            *match = SIM_ROM_PATCH_MATCH_NO;
    }

    return SCPE_OK;
}

/*
 * Write one complete byte sequence to a ROM patch target range.  The caller is
 * responsible for verifying that the current ROM contents are safe to replace.
 */
static t_stat sim_rom_patch_write(const sim_rom_patch *patch,
                                  sim_rom_patch_write_byte write_byte,
                                  void *context, const uint8_t *bytes)
{
    for (size_t i = 0; i < patch->size; i++) {
        t_stat r = write_byte(context, patch->address + i, bytes[i]);

        if (r != SCPE_OK)
            return r;
    }

    return SCPE_OK;
}

/*
 * Return whether a ROM patch descriptor and its access callbacks are usable.
 */
static bool sim_rom_patch_valid(const sim_rom_patch *patch,
                                sim_rom_patch_read_byte read_byte,
                                sim_rom_patch_write_byte write_byte)
{
    return patch != NULL && read_byte != NULL && write_byte != NULL &&
           patch->expected != NULL && patch->replacement != NULL &&
           patch->size != 0;
}

t_stat sim_rom_patch_apply(const sim_rom_patch *patch,
                           sim_rom_patch_read_byte read_byte,
                           sim_rom_patch_write_byte write_byte, void *context)
{
    enum sim_rom_patch_match match;
    t_stat r;

    if (!sim_rom_patch_valid(patch, read_byte, write_byte))
        return SCPE_ARG;

    r = sim_rom_patch_matches(patch, read_byte, context, patch->expected,
                              &match);
    if (r != SCPE_OK)
        return r;
    if (match == SIM_ROM_PATCH_MATCH_YES)
        return sim_rom_patch_write(patch, write_byte, context,
                                   patch->replacement);

    r = sim_rom_patch_matches(patch, read_byte, context, patch->replacement,
                              &match);
    if (r != SCPE_OK)
        return r;
    if (match == SIM_ROM_PATCH_MATCH_YES)
        return SCPE_OK;

    return SCPE_ARG;
}

t_stat sim_rom_patch_revert(const sim_rom_patch *patch,
                            sim_rom_patch_read_byte read_byte,
                            sim_rom_patch_write_byte write_byte, void *context)
{
    enum sim_rom_patch_match match;
    t_stat r;

    if (!sim_rom_patch_valid(patch, read_byte, write_byte))
        return SCPE_ARG;

    r = sim_rom_patch_matches(patch, read_byte, context, patch->replacement,
                              &match);
    if (r != SCPE_OK)
        return r;
    if (match == SIM_ROM_PATCH_MATCH_YES)
        return sim_rom_patch_write(patch, write_byte, context, patch->expected);

    r = sim_rom_patch_matches(patch, read_byte, context, patch->expected,
                              &match);
    if (r != SCPE_OK)
        return r;
    if (match == SIM_ROM_PATCH_MATCH_YES)
        return SCPE_OK;

    return SCPE_ARG;
}
