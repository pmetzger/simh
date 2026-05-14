/* sim_imd.h: ImageDisk disk image support */
// SPDX-FileCopyrightText: 2007-2022 Howard M. Harte
// SPDX-License-Identifier: MIT

#ifndef SIM_IMD_H_
#define SIM_IMD_H_     1

#include <stdint.h>

#include "sim_defs.h"

typedef struct {
    uint8_t mode;
    uint8_t cyl;
    uint8_t head;
    uint8_t nsects;
    uint8_t sectsize;
} IMD_HEADER;


#define IMD_FLAG_SECT_HEAD_MAP  (1 << 6)
#define IMD_FLAG_SECT_CYL_MAP   (1 << 7)

#define SECT_RECORD_UNAVAILABLE         0   /* Data could not be read from the original media */
#define SECT_RECORD_NORM                1   /* Normal Data */
#define SECT_RECORD_NORM_COMP           2   /* Compressed Normal Data */
#define SECT_RECORD_NORM_DAM            3   /* Normal Data with deleted address mark */
#define SECT_RECORD_NORM_DAM_COMP       4   /* Compressed Normal Data with deleted address mark */
#define SECT_RECORD_NORM_ERR            5   /* Normal Data */
#define SECT_RECORD_NORM_COMP_ERR       6   /* Compressed Normal Data */
#define SECT_RECORD_NORM_DAM_ERR        7   /* Normal Data with deleted address mark */
#define SECT_RECORD_NORM_DAM_COMP_ERR   8   /* Compressed Normal Data with deleted address mark */

#define MAX_CYL     80
#define MAX_HEAD    2
#define MAX_SPT     26

#define FD_FLAG_WRITELOCK   1

#define IMD_DISK_IO_ERROR_GENERAL       (1 << 0)    /* General data error. */
#define IMD_DISK_IO_ERROR_CRC           (1 << 1)    /* Data read/written, but got a CRC error. */
#define IMD_DISK_IO_DELETED_ADDR_MARK   (1 << 2)    /* Sector had a deleted address mark */
#define IMD_DISK_IO_COMPRESSED          (1 << 3)    /* Sector is compressed in the IMD file (Read Only) */
#define IMD_DISK_IO_ERROR_WPROT         (1 << 4)    /* Disk is write protected */

#define IMD_MODE_500K_FM        0
#define IMD_MODE_300K_FM        1
#define IMD_MODE_250K_FM        2
#define IMD_MODE_500K_MFM       3
#define IMD_MODE_300K_MFM       4
#define IMD_MODE_250K_MFM       5

#define IMD_MAX_SECTSIZE        6

#define IMD_MODE_FM(x)      (x <= IMD_MODE_250K_FM)
#define IMD_MODE_MFM(x)     (x >= IMD_MODE_500K_MFM)

#define IMAGE_TYPE_DSK          1               /* Flat binary "DSK" image file.            */
#define IMAGE_TYPE_IMD          2               /* ImageDisk "IMD" image file.              */
#define IMAGE_TYPE_CPT          3               /* CP/M Transfer "CPT" image file.          */

typedef struct {
    uint8_t mode;
    uint8_t nsects;
    uint32_t sectsize;
    uint32_t sectorOffsetMap[MAX_SPT];
    uint8_t start_sector;
    uint8_t logicalHead[MAX_SPT];
    uint8_t logicalCyl[MAX_SPT];
} TRACK_INFO;

typedef struct {
    FILE *file;
    uint32_t ntracks;
    uint8_t nsides;
    uint8_t flags;
    DEVICE *device;
    uint32_t debugmask;
    uint32_t verbosedebugmask;
    TRACK_INFO track[MAX_CYL][MAX_HEAD];
} DISK_INFO;

extern DISK_INFO *diskOpen(FILE *fileref, uint32_t isVerbose);
extern DISK_INFO *diskOpenEx(FILE *fileref, uint32_t isVerbose, DEVICE *device, uint32_t debugmask, uint32_t verbosedebugmask);
extern t_stat diskClose(DISK_INFO **myDisk);
extern t_stat diskCreate(FILE *fileref, const char *ctlr_comment);
extern uint32_t imdGetSides(DISK_INFO *myDisk);
extern uint32_t imdIsWriteLocked(DISK_INFO *myDisk);

extern t_stat sectSeek(DISK_INFO *myDisk, uint32_t Cyl, uint32_t Head);
extern t_stat sectRead(DISK_INFO *myDisk, uint32_t Cyl, uint32_t Head, uint32_t Sector, uint8_t *buf, uint32_t buflen, uint32_t *flags, uint32_t *readlen);
extern t_stat sectWrite(DISK_INFO *myDisk, uint32_t Cyl, uint32_t Head, uint32_t Sector, uint8_t *buf, uint32_t buflen, uint32_t *flags, uint32_t *writelen);
extern t_stat trackWrite(DISK_INFO *myDisk,
               uint32_t Cyl,
               uint32_t Head,
               uint32_t numSectors,
               uint32_t sectorLen,
               uint8_t *sectorMap,
               uint8_t mode,
               uint8_t fillbyte,
               uint32_t *flags);
extern t_stat assignDiskType(UNIT *uptr);

#endif
