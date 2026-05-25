/* vax_kdb50_internal.h: internal VAXBI KDB50 definitions */
// SPDX-FileCopyrightText: 2026 The ZIMH contributors
// SPDX-License-Identifier: MIT

#ifndef VAX_KDB50_INTERNAL_H_
#define VAX_KDB50_INTERNAL_H_ 1

#define KDB50_DEFAULT_NEXUS 5u

#define KDB50_IP_OF 0xF2u
#define KDB50_SA_READ_OF 0xF4u
#define KDB50_SA_WRITE_OF 0xF6u

#define KDB50_RA81_BLOCKS 891072u

#define KDB50_MSCP_CONTROLLER_MODEL 18u
#define KDB50_MSCP_HW_VERSION 1u
#define KDB50_MSCP_SW_VERSION 3u
#define KDB50_MSCP_DEFAULT_TIMEOUT 120u
#define KDB50_MSCP_MAX_UNITS 4u
#define KDB50_MSCP_RA81_UNIT_MODEL 5u
#define KDB50_MSCP_RA81_SECTORS_PER_TRACK 51u
#define KDB50_MSCP_RA81_TRACKS_PER_GROUP 14u
#define KDB50_MSCP_RA81_GROUPS_PER_CYLINDER 1u
#define KDB50_MSCP_RA81_MEDIA_ID 0x25641051u

/*
 * KDB50 User Guide EK-KDB50-UG-PRE describes IP/SA through BIIC GPR0/GPR1
 * in section 3.2.3.  Its VAXBI mapped transfers use the SSP mapped-buffer
 * descriptor format; SSP 3.1 section 2.3.7.2 defines the map-base walk.
 */
#define KDB50_MSCP_BUFFER_MAP 0x80000000u
#define KDB50_MSCP_PHYS_ADDR_MASK 0x1fffffffu
#define KDB50_MSCP_BUS_ADDR_MAX 0x3fffffffu
#define KDB50_MSCP_MAPPED_MAPBASE_INVALID 0xc0000003u
#define KDB50_MSCP_MAPPED_DESCRIPTOR_MAX 0x7fffffffu
#define KDB50_MSCP_UNMAPPED_MAX_ADDR 0x3ffffffeu
#define KDB50_MSCP_PTE_VALID 0x80000000u
#define KDB50_MSCP_PTE_PFN_MASK 0x001fffffu
#define KDB50_MSCP_PAGE_BYTES 512u

#endif /* VAX_KDB50_INTERNAL_H_ */
