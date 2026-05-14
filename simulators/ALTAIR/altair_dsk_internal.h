/* altair_dsk_internal.h: private 88-DISK helpers for tests */

#ifndef ALTAIR_DSK_INTERNAL_H_
#define ALTAIR_DSK_INTERNAL_H_ 1

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "altair_defs.h"

typedef int (*altair_dsk_seek_fn)(FILE *file, long offset, int whence);
typedef size_t (*altair_dsk_read_fn)(void *ptr, size_t size, size_t count,
                                     FILE *file);
typedef size_t (*altair_dsk_write_fn)(const void *ptr, size_t size,
                                      size_t count, FILE *file);

typedef struct {
    altair_dsk_seek_fn seek;
    altair_dsk_read_fn read;
    altair_dsk_write_fn write;
} ALTAIR_DSK_IO_HOOKS;

void altair_dsk_set_io_hooks(const ALTAIR_DSK_IO_HOOKS *hooks);
void altair_dsk_reset_io_hooks(void);

t_stat dsk_reset(DEVICE *dptr);
bool writebuf(void);

#endif /* ALTAIR_DSK_INTERNAL_H_ */
