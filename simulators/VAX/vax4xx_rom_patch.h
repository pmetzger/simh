/* vax4xx_rom_patch.h: KA4xx ROM patch interfaces */
// SPDX-FileCopyrightText: 2026 The ZIMH Contributors
// SPDX-License-Identifier: MIT

#ifndef VAX4XX_ROM_PATCH_H_
#define VAX4XX_ROM_PATCH_H_ 1

#include <stdio.h>

#include "vax_defs.h"

/*
 * Apply any enabled KA4xx ROM patches to a loaded ROM image.  If the ROM has
 * not been loaded yet, the enabled patch setting remains pending.
 */
t_stat rom_apply_patches(void);

/* Set the currently enabled KA4xx ROM patch by name. */
t_stat rom_set_patch(UNIT *uptr, int32_t val, const char *cptr, void *desc);

/* Show the currently enabled KA4xx ROM patch. */
t_stat rom_show_patch(FILE *st, UNIT *uptr, int32_t val, const void *desc);

#endif /* VAX4XX_ROM_PATCH_H_ */
