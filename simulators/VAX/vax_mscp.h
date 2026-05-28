/* vax_mscp.h: VAX MSCP storage controller engine */
// SPDX-FileCopyrightText: 2026 The ZIMH contributors
// SPDX-License-Identifier: MIT

#ifndef VAX_MSCP_H_
#define VAX_MSCP_H_ 1

#include <stdbool.h>
#include <stdint.h>

#define VAX_MSCP_MAX_UNITS 4u
#define VAX_MSCP_FORCED_ERROR_MAX_BLOCKS 891072u
#define VAX_MSCP_FORCED_ERROR_BYTES                                            \
    ((VAX_MSCP_FORCED_ERROR_MAX_BLOCKS + 7u) / 8u)

typedef int (*vax_mscp_read_words_fn)(void *ctx, uint32_t addr, uint16_t *buf,
                                      uint32_t bytes);
typedef int (*vax_mscp_read_bytes_fn)(void *ctx, uint32_t addr, uint8_t *buf,
                                      uint32_t bytes);
typedef int (*vax_mscp_write_words_fn)(void *ctx, uint32_t addr,
                                       const uint16_t *buf, uint32_t bytes);
typedef int (*vax_mscp_write_bytes_fn)(void *ctx, uint32_t addr,
                                       const uint8_t *buf, uint32_t bytes);
typedef int (*vax_mscp_read_sectors_fn)(void *ctx, uint16_t unit, uint32_t lbn,
                                        uint8_t *buf, uint32_t sectors);
typedef int (*vax_mscp_write_sectors_fn)(void *ctx, uint16_t unit, uint32_t lbn,
                                         const uint8_t *buf, uint32_t sectors);
typedef int (*vax_mscp_read_map_pte_fn)(void *ctx, uint32_t mapbase,
                                        uint32_t page_index, uint32_t *pte);
typedef void (*vax_mscp_ring_interrupt_fn)(void *ctx);

typedef struct {
    uint16_t controller_model;
    uint16_t controller_hw_version;
    uint16_t controller_sw_version;
    uint16_t controller_timeout;
    uint8_t interrupt_vector_external;
    uint8_t max_units;
    uint16_t unit_model;
    uint16_t sectors_per_track;
    uint16_t tracks_per_group;
    uint16_t groups_per_cylinder;
    uint16_t unit_version;
    uint32_t media_id;
    uint32_t buffer_map_bit;
    uint32_t phys_addr_mask;
    uint32_t bus_addr_max;
    uint32_t mapped_mapbase_invalid;
    uint32_t mapped_descriptor_max;
    uint32_t unmapped_max_addr;
    uint32_t pte_valid;
    uint32_t pte_pfn_mask;
    uint32_t page_bytes;
    uint32_t forced_error_blocks;
} vax_mscp_profile;

typedef struct {
    void *ctx;
    const vax_mscp_profile *profile;
    vax_mscp_read_words_fn read_words;
    vax_mscp_read_bytes_fn read_bytes;
    vax_mscp_write_words_fn write_words;
    vax_mscp_write_bytes_fn write_bytes;
    vax_mscp_read_sectors_fn read_sectors;
    vax_mscp_write_sectors_fn write_sectors;
    vax_mscp_read_map_pte_fn read_map_pte;
    vax_mscp_ring_interrupt_fn ring_interrupt;
} vax_mscp_bus;

typedef struct {
    uint8_t present;
    uint8_t online;
    uint8_t compare_reads;
    uint8_t compare_writes;
    uint8_t write_protected;
    uint8_t forced_error[VAX_MSCP_FORCED_ERROR_BYTES];
    uint32_t blocks;
} vax_mscp_unit;

typedef struct {
    /*
     * vax_mscp_reset_with_bus() preserves bus, profile, unit[],
     * last_fail_code, and last_fail_valid. Update its explicit reset list
     * when adding reset-transient fields here.
     */
    vax_mscp_bus bus;
    const vax_mscp_profile *profile;
    uint16_t sa;
    uint16_t saw;
    uint16_t s1;
    uint16_t interrupt_vector;
    uint32_t comm;
    uint32_t cmd_index;
    uint32_t rsp_index;
    uint32_t state;
    uint16_t last_fail_code;
    uint8_t purge_interrupt;
    uint8_t last_fail_valid;
    uint8_t last_fail_pending;
    vax_mscp_unit unit[VAX_MSCP_MAX_UNITS];
} vax_mscp;

bool vax_mscp_profile_valid(const vax_mscp_profile *profile);
/* Initialize an MSCP engine; returns false if ctlr, bus, or profile is invalid.
 */
bool vax_mscp_init(vax_mscp *ctlr, const vax_mscp_bus *bus);
bool vax_mscp_reset(vax_mscp *ctlr);
bool vax_mscp_reset_with_bus(vax_mscp *ctlr, const vax_mscp_bus *bus);
uint16_t vax_mscp_read_ip(vax_mscp *ctlr);
uint16_t vax_mscp_read_sa(const vax_mscp *ctlr);
uint16_t vax_mscp_interrupt_vector(const vax_mscp *ctlr);
void vax_mscp_write_sa(vax_mscp *ctlr, uint16_t value);
uint32_t vax_mscp_comm_region(const vax_mscp *ctlr);
void vax_mscp_set_unit(vax_mscp *ctlr, uint16_t unit, uint32_t blocks);

#endif
