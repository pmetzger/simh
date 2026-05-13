/* vax4xx_rom_patch.c: KA4xx ROM patch support */
// SPDX-FileCopyrightText: 2026 The ZIMH Contributors
// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "sim_fio.h"
#include "sim_rom_patch.h"
#include "uint_bits.h"
#include "vax_defs.h"
#include "vax4xx_rom_patch.h"

static const uint8_t ka48_skip_ram_selftest_expected[] = {
    0xc2,
    0x2c,
    0x5e,
    0xd0,
};

static const uint8_t ka48_skip_ram_selftest_replacement[] = {
    0xd0,
    0x01,
    0x50,
    0x04,
};

static const sim_rom_patch ka48_rom_patches[] = {
    {
        "SKIP-RAM-SELFTEST",
        "Skip the KA48 RAM self-test",
        0x2004de0a,
        ka48_skip_ram_selftest_expected,
        ka48_skip_ram_selftest_replacement,
        sizeof(ka48_skip_ram_selftest_expected),
    },
};

enum {
    KA48_ROM_PATCH_COUNT =
        sizeof(ka48_rom_patches) / sizeof(ka48_rom_patches[0]),
};

static bool rom_enabled_patches[KA48_ROM_PATCH_COUNT];
static bool rom_applied_patches[KA48_ROM_PATCH_COUNT];

/*
 * Return whether the KA4xx ROM buffer contains a loaded image.  The boot
 * paths already use word zero as the loaded-ROM sentinel; patch application
 * follows that convention so SET ROM PATCH can be issued before BOOT.
 */
static bool rom_is_loaded(void)
{
    return rom != NULL && *rom != 0;
}

/*
 * Validate that a guest ROM address is accessible through the KA4xx ROM
 * buffer before a patch helper reads or writes it.
 */
static t_stat rom_patch_check_address(t_addr address)
{
    if ((address < ROMBASE) || (address >= (ROMBASE + ROMSIZE)))
        return SCPE_NXM;
    if (rom == NULL)
        return SCPE_IERR;
    return SCPE_OK;
}

/*
 * Read one byte from the KA4xx ROM buffer for the generic ROM patch helper.
 */
static t_stat rom_patch_read_byte(void *context, t_addr address, uint8_t *value)
{
    uint32_t offset;
    t_stat r;
    (void)context;

    if (value == NULL)
        return SCPE_ARG;

    r = rom_patch_check_address(address);
    if (r != SCPE_OK)
        return r;

    offset = (uint32_t)(address - ROMBASE);
    *value = (uint8_t)u32_get_addr_u8_le(rom[offset >> 2], (uint32_t)address);
    return SCPE_OK;
}

/*
 * Write one byte to the KA4xx ROM buffer for the generic ROM patch helper.
 */
static t_stat rom_patch_write_byte(void *context, t_addr address, uint8_t value)
{
    t_stat r;
    (void)context;

    r = rom_patch_check_address(address);
    if (r != SCPE_OK)
        return r;

    rom_wr_B((int32_t)address, value);
    return SCPE_OK;
}

/*
 * Disable every configured KA4xx ROM patch.
 */
static void rom_clear_enabled_patches(void)
{
    for (size_t i = 0; i < KA48_ROM_PATCH_COUNT; i++)
        rom_enabled_patches[i] = false;
}

/*
 * Find a named KA4xx ROM patch.  Names are accepted case-insensitively to
 * match normal simulator command parsing expectations.
 */
static const sim_rom_patch *rom_find_patch(const char *name, size_t *index)
{
    for (size_t i = 0; i < KA48_ROM_PATCH_COUNT; i++) {
        if (strcasecmp(name, ka48_rom_patches[i].name) == 0) {
            *index = i;
            return &ka48_rom_patches[i];
        }
    }

    return NULL;
}

/*
 * Apply or revert KA4xx ROM patches to make the loaded ROM match the current
 * patch setting.  If the ROM has not been loaded yet, the setting remains
 * pending and will be applied by the boot path after cpu_load_bootcode().
 */
t_stat rom_apply_patches(void)
{
    if (!rom_is_loaded())
        return SCPE_OK;

    for (size_t i = 0; i < KA48_ROM_PATCH_COUNT; i++) {
        const sim_rom_patch *patch = &ka48_rom_patches[i];
        t_stat r;

        if (rom_enabled_patches[i]) {
            r = sim_rom_patch_apply(patch, rom_patch_read_byte,
                                    rom_patch_write_byte, NULL);
            if (r != SCPE_OK)
                return sim_messagef(r, "ROM patch %s does not match ROM\n",
                                    patch->name);
            rom_applied_patches[i] = true;
        } else if (rom_applied_patches[i]) {
            r = sim_rom_patch_revert(patch, rom_patch_read_byte,
                                     rom_patch_write_byte, NULL);
            if (r != SCPE_OK)
                return sim_messagef(r, "ROM patch %s cannot be reverted\n",
                                    patch->name);
            rom_applied_patches[i] = false;
        }
    }

    return SCPE_OK;
}

/*
 * Set the enabled KA4xx ROM patch by name.  A set made before the ROM image is
 * loaded is remembered and applied after the image is loaded by BOOT.
 */
t_stat rom_set_patch(UNIT *uptr, int32_t val, const char *cptr, void *desc)
{
    bool old_enabled[KA48_ROM_PATCH_COUNT];
    size_t patch_index = 0;
    const sim_rom_patch *patch;
    t_stat r;
    (void)uptr;
    (void)val;
    (void)desc;

    if ((cptr == NULL) || (*cptr == '\0'))
        return SCPE_2FARG;

    memcpy(old_enabled, rom_enabled_patches, sizeof(old_enabled));

    if (strcasecmp(cptr, "NONE") == 0) {
        rom_clear_enabled_patches();
        r = rom_apply_patches();
        if (r != SCPE_OK)
            memcpy(rom_enabled_patches, old_enabled,
                   sizeof(rom_enabled_patches));
        return r;
    }

    patch = rom_find_patch(cptr, &patch_index);
    if (patch == NULL)
        return sim_messagef(SCPE_ARG, "Unknown ROM patch: %s\n", cptr);

    rom_clear_enabled_patches();
    rom_enabled_patches[patch_index] = true;
    r = rom_apply_patches();
    if (r != SCPE_OK) {
        memcpy(rom_enabled_patches, old_enabled, sizeof(rom_enabled_patches));
        return r;
    }

    return SCPE_OK;
}

/*
 * Show the enabled KA4xx ROM patch setting.
 */
t_stat rom_show_patch(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    (void)uptr;
    (void)val;
    (void)desc;

    for (size_t i = 0; i < KA48_ROM_PATCH_COUNT; i++) {
        if (rom_enabled_patches[i]) {
            fprintf(st, "patch=%s", ka48_rom_patches[i].name);
            return SCPE_OK;
        }
    }

    fprintf(st, "patch=none");
    return SCPE_OK;
}
