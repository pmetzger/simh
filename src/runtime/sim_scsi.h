/* sim_scsi.h: SCSI bus simulation */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-License-Identifier: X11

#ifndef _SIM_SCSI_H_
#define _SIM_SCSI_H_     0

#include <stdbool.h>
#include <stdint.h>

#include "sim_defs.h"
#include "sim_disk.h"
#include "sim_tape.h"

/* SCSI device states */

#define SCSI_DISC       0                               /* disconnected */
#define SCSI_TARG       1                               /* target mode */
#define SCSI_INIT       2                               /* initiator mode */

/* SCSI device types */

#define SCSI_DISK       0                               /* direct access device */
#define SCSI_TAPE       1                               /* sequential access device */
#define SCSI_PRINT      2                               /* printer */
#define SCSI_PROC       3                               /* processor */
#define SCSI_WORM       4                               /* write-once device */
#define SCSI_CDROM      5                               /* CD-ROM */
#define SCSI_SCAN       6                               /* scanner */
#define SCSI_OPTI       7                               /* optical */
#define SCSI_JUKE       8                               /* jukebox */
#define SCSI_COMM       9                               /* communications device */

/* SCSI bus phases */

#define SCSI_DATO       0                               /* data out */
#define SCSI_DATI       1                               /* data in */
#define SCSI_CMD        2                               /* command */
#define SCSI_STS        3                               /* status */
#define SCSI_MSGO       6                               /* message out */
#define SCSI_MSGI       7                               /* message in */

/* Debugging bitmaps */

#define SCSI_DBG_CMD    0x01000000                      /* SCSI commands */
#define SCSI_DBG_MSG    0x02000000                      /* SCSI messages */
#define SCSI_DBG_BUS    0x04000000                      /* bus activity */
#define SCSI_DBG_DSK    0x08000000                      /* disk activity */
#define SCSI_DBG_TAP    0x10000000                      /* tape activity */

#define SCSI_V_NOAUTO   ((DKUF_V_UF > MTUF_V_UF) ? DKUF_V_UF : MTUF_V_UF)/* noautosize */
#define SCSI_V_QIC      (SCSI_V_NOAUTO + 1)
#define SCSI_V_UF       (SCSI_V_QIC + 1)
#define SCSI_QIC        (1 << SCSI_V_QIC)
#define SCSI_WLK        (UNIT_WLK|UNIT_RO)              /* hwre write lock */
#define SCSI_NOAUTO     DKUF_NOAUTOSIZE

#define SCSI_QIC_BLKSZ  0x200

struct scsi_dev_t {
    uint8_t devtype;                                    /* device type */
    uint8_t pqual;                                      /* peripheral qualifier */
    uint32_t scsiver;                                   /* SCSI version */
    bool removeable;                                    /* removable flag */
    uint32_t block_size;                                /* device block size */
    uint32_t lbn;                                       /* device size (blocks) */
    const char *manufacturer;                           /* manufacturer string */
    const char *product;                                /* product string */
    const char *rev;                                    /* revision string */
    const char *name;                                   /* gap length for tapes */
    uint32_t gaplen;
    };

struct scsi_bus_t {
    DEVICE *dptr;                                       /* SCSI device */
    UNIT *dev[8];                                       /* target units */
    int32_t initiator;                                  /* current initiator */
    int32_t target;                                     /* current target */
    bool atn;                                           /* attention flag */
    bool req;                                           /* request flag */
    uint8_t *buf;                                       /* transfer buffer */
    uint8_t cmd[10];                                    /* command buffer */
    uint32_t buf_b;                                     /* buffer bottom ptr */
    uint32_t buf_t;                                     /* buffer top ptr */
    uint32_t phase;                                     /* current bus phase */
    uint32_t lun;                                       /* selected lun */
    uint32_t status;
    uint32_t sense_key;
    uint32_t sense_code;
    uint32_t sense_qual;
    uint32_t sense_info;
};

typedef struct scsi_bus_t SCSI_BUS;
typedef struct scsi_dev_t SCSI_DEV;

bool scsi_arbitrate (SCSI_BUS *bus, uint32_t initiator);
void scsi_release (SCSI_BUS *bus);
void scsi_set_atn (SCSI_BUS *bus);
void scsi_release_atn (SCSI_BUS *bus);
bool scsi_select (SCSI_BUS *bus, uint32_t target);
uint32_t scsi_write (SCSI_BUS *bus, uint8_t *data, uint32_t len);
uint32_t scsi_read (SCSI_BUS *bus, uint8_t *data, uint32_t len);
uint32_t scsi_state (SCSI_BUS *bus, uint32_t id);
void scsi_add_unit (SCSI_BUS *bus, uint32_t id, UNIT *uptr);
void scsi_set_unit (SCSI_BUS *bus, UNIT *uptr, SCSI_DEV *dev);
void scsi_reset_unit (UNIT *uptr);
void scsi_reset (SCSI_BUS *bus);
t_stat scsi_init (SCSI_BUS *bus, uint32_t maxfr);

t_stat scsi_set_fmt (UNIT *uptr, int32_t val, const char *cptr, void *desc);
t_stat scsi_set_wlk (UNIT *uptr, int32_t val, const char *cptr, void *desc);
t_stat scsi_show_fmt (FILE *st, UNIT *uptr, int32_t val, const void *desc);
t_stat scsi_show_wlk (FILE *st, UNIT *uptr, int32_t val, const void *desc);
t_stat scsi_attach (UNIT *uptr, const char *cptr);
t_stat scsi_attach_ex (UNIT *uptr, const char *cptr, const char **drivetypes);
t_stat scsi_detach (UNIT *uptr);
t_stat scsi_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);

#endif
