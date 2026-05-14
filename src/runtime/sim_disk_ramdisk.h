/* sim_disk_ramdisk.h: private ramdisk support for sim_disk */
// SPDX-FileCopyrightText: 2026 The ZIMH Contributors
// SPDX-License-Identifier: MIT

#ifndef SIM_DISK_RAMDISK_H_
#define SIM_DISK_RAMDISK_H_ 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "sim_defs.h"

typedef struct sim_disk_ramdisk sim_disk_ramdisk;
typedef struct sim_disk_ramdisk_spec sim_disk_ramdisk_spec;

/* Return true when an attach argument names an in-memory RAMDISK:. */
bool sim_disk_ramdisk_is_spec(const char *spec);

/* Reject attach switches that only make sense for disk container files. */
t_stat sim_disk_ramdisk_reject_container_switches(UNIT *uptr);

/* Report that this build cannot provide RAMDISK: attachments. */
t_stat sim_disk_ramdisk_unavailable(UNIT *uptr);

#if defined(HAVE_FMEMOPEN)
/* Parse the RAMDISK: attach argument into validated option fields. */
t_stat sim_disk_ramdisk_parse_spec(const char *spec,
                                   sim_disk_ramdisk_spec **parsed);

/* Release a parsed RAMDISK: attach argument. */
void sim_disk_ramdisk_free_spec(sim_disk_ramdisk_spec *spec);

/* Return true when the attach argument explicitly supplies TYPE=. */
bool sim_disk_ramdisk_spec_has_type(const sim_disk_ramdisk_spec *spec);

/* Return the parsed TYPE= value. */
const char *sim_disk_ramdisk_spec_type(const sim_disk_ramdisk_spec *spec);

/* Create the memory buffer and FILE stream for a RAMDISK: attachment. */
t_stat sim_disk_ramdisk_create(UNIT *uptr, const sim_disk_ramdisk_spec *spec,
                               t_offset default_size, uint32_t sector_size,
                               bool restoring, const char *mode, FILE **fileref,
                               sim_disk_ramdisk **ramdisk);
#endif

/* Return the byte size of a live ramdisk attachment. */
t_offset sim_disk_ramdisk_size(const sim_disk_ramdisk *ramdisk);

/* Persist a live ramdisk attachment to its SAVE= image. */
t_stat sim_disk_ramdisk_save(UNIT *uptr, const sim_disk_ramdisk *ramdisk);

/* Release the resources owned by a live ramdisk attachment. */
void sim_disk_ramdisk_free(sim_disk_ramdisk *ramdisk);

#endif
