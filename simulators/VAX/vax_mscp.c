/* vax_mscp.c: VAX MSCP storage controller engine */
// SPDX-FileCopyrightText: 2026 The ZIMH contributors
// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "uint_bits.h"
#include "vax_mscp.h"

#define UQSSP_ERR 0x8000u
#define UQSSP_STEP4 0x4000u
#define UQSSP_STEP3 0x2000u
#define UQSSP_STEP2 0x1000u
#define UQSSP_STEP1 0x0800u
#define UQSSP_DI 0x0100u
#define UQSSP_MP 0x0040u

#define UQSSP_S1_VALID 0x8000u
#define UQSSP_S1_WRAP 0x4000u
#define UQSSP_S1_CQ_SHIFT 11u
#define UQSSP_S1_RQ_SHIFT 8u
#define UQSSP_S1_Q_MASK 0x7u
#define UQSSP_S1_IE 0x0080u
#define UQSSP_S1_VECTOR_MASK 0x007fu
#define UQSSP_S1_STEP3_ECHO_MASK 0x00ffu

#define UQSSP_S2_COMM_LO_MASK 0xfffeu
#define UQSSP_S2_PURGE_INTERRUPT 0x0001u
#define UQSSP_S3_PURGE_POLL 0x8000u
#define UQSSP_S3_COMM_HI_MASK 0x7fffu
#define UQSSP_S4_GO 0x0001u
#define UQSSP_S4_LF 0x0002u

#define UQSSP_PORT_VERSION 3u

#define UQSSP_COMM_QQ_OFFSET (-8)
#define UQSSP_COMM_CI_OFFSET (-4)
#define UQSSP_COMM_RI_OFFSET (-2)

#define UQSSP_FATAL_PACKET_READ 1u
#define UQSSP_FATAL_PACKET_WRITE 2u
#define UQSSP_FATAL_QUEUE_READ 6u
#define UQSSP_FATAL_QUEUE_WRITE 7u
#define UQSSP_FATAL_INVALID_CONNECTION_ID 14u
#define UQSSP_FATAL_INTERRUPT_WRITE 15u
#define UQSSP_FATAL_PROTOCOL_INCOMPATIBILITY 20u
#define UQSSP_FATAL_PURGE_POLL 21u

#define UQ_DESC_OWN 0x80000000u
#define UQ_DESC_FLAG 0x40000000u
#define UQ_HDR_OFF 4u
#define UQ_PACKET_WORDS 32u

#define UQ_WORD_LENGTH 0u
#define UQ_WORD_CREDITS_TYPE 1u
#define UQ_WORD_REF_LOW 2u
#define UQ_WORD_REF_HIGH 3u
#define UQ_WORD_UNIT 4u
#define UQ_WORD_RESERVED 5u
#define UQ_WORD_OPCODE 6u
#define UQ_WORD_STATUS 7u

#define UQ_TYPE_SEQ 0u
#define UQ_TYPE_DATAGRAM 1u
#define UQ_CONNECTION_ID_DISK 0u
#define MSCP_SCC_CREDITS 2u
#define MSCP_RESPONSE_CREDITS 1u
#define MSCP_OP_ABORT 1u
#define MSCP_OP_GCS 2u
#define MSCP_OP_GUS 3u
#define MSCP_OP_SCC 4u
#define MSCP_OP_DISPLAY 6u
#define MSCP_OP_AVAILABLE 8u
#define MSCP_OP_ONL 9u
#define MSCP_OP_SETUNITC 10u
#define MSCP_OP_DAP 11u
#define MSCP_OP_ACCESS 16u
#define MSCP_OP_ERASE 18u
#define MSCP_OP_NOP17 17u
#define MSCP_OP_NOP19 19u
#define MSCP_OP_COMPARE 32u
#define MSCP_OP_READ 33u
#define MSCP_OP_WRITE 34u
#define MSCP_OP_END 0x80u
#define MSCP_MODIFIER_BYTE_OFFSET 10u
#define MSCP_ONL_RESERVED_BYTE_OFFSET 12u
#define MSCP_SHADOW_RESERVED_BYTE_OFFSET 32u
#define MSCP_BYTE_COUNT_BYTE_OFFSET 12u
#define MSCP_BUFFER_BYTE_OFFSET 16u
#define MSCP_LBN_BYTE_OFFSET 28u
#define MSCP_OPCODE_BYTE_OFFSET 8u
#define MSCP_RESERVED_BYTE_OFFSET 6u
#define MSCP_GUS_NEXT_UNIT 1u
#define MSCP_STATUS_SUCCESS 0u
#define MSCP_STATUS_SUCCESS_ALREADY_ONLINE 0x0100u
#define MSCP_STATUS_COMPARE_ERROR 7u
#define MSCP_STATUS_DATA_ERROR_FORCE_ERROR 8u
#define MSCP_STATUS_HOST_ERROR 9u
#define MSCP_STATUS_HOST_ODD_ADDRESS 0x0029u
#define MSCP_STATUS_HOST_ODD_BYTE_COUNT 0x0049u
#define MSCP_STATUS_HOST_INVALID_PTE 0x00a9u
#define MSCP_STATUS_OFFLINE 3u
#define MSCP_STATUS_AVAILABLE 4u
#define MSCP_STATUS_WRITE_PROTECTED_SOFTWARE 0x1006u
#define MSCP_STATUS_INVALID_COMMAND 1u
#define MSCP_STATUS_INVALID_MESSAGE_LENGTH MSCP_STATUS_INVALID_COMMAND
#define MSCP_STATUS_INVALID_BYTE_COUNT                                         \
    ((MSCP_BYTE_COUNT_BYTE_OFFSET << 8) | MSCP_STATUS_INVALID_COMMAND)
#define MSCP_STATUS_INVALID_ITEM MSCP_STATUS_INVALID_BYTE_COUNT
#define MSCP_STATUS_INVALID_MSCP_VERSION MSCP_STATUS_INVALID_BYTE_COUNT
#define MSCP_STATUS_INVALID_LBN                                                \
    ((MSCP_LBN_BYTE_OFFSET << 8) | MSCP_STATUS_INVALID_COMMAND)
#define MSCP_STATUS_INVALID_MODIFIER                                           \
    ((MSCP_MODIFIER_BYTE_OFFSET << 8) | MSCP_STATUS_INVALID_COMMAND)
#define MSCP_STATUS_INVALID_ONL_RESERVED_FIELD                                 \
    ((MSCP_ONL_RESERVED_BYTE_OFFSET << 8) | MSCP_STATUS_INVALID_COMMAND)
#define MSCP_STATUS_INVALID_OPCODE                                             \
    ((MSCP_OPCODE_BYTE_OFFSET << 8) | MSCP_STATUS_INVALID_COMMAND)
#define MSCP_STATUS_INVALID_RESERVED_FIELD                                     \
    ((MSCP_RESERVED_BYTE_OFFSET << 8) | MSCP_STATUS_INVALID_COMMAND)
#define MSCP_STATUS_INVALID_BUFFER_RESERVED_FIELD                              \
    ((MSCP_BUFFER_BYTE_OFFSET << 8) | MSCP_STATUS_INVALID_COMMAND)
#define MSCP_STATUS_INVALID_SHADOW_RESERVED_FIELD                              \
    ((MSCP_SHADOW_RESERVED_BYTE_OFFSET << 8) | MSCP_STATUS_INVALID_COMMAND)
#define MSCP_LAST_FAIL_LENGTH 24u
#define MSCP_SCC_LENGTH 32u
#define MSCP_AVAILABLE_LENGTH 12u
#define MSCP_GCS_LENGTH 20u
#define MSCP_GUS_LENGTH 48u
#define MSCP_MIN_DISPLAY_LENGTH 16u
#define MSCP_MIN_ONL_LENGTH 36u
#define MSCP_MIN_RW_LENGTH 32u
#define MSCP_ONL_LENGTH 44u
#define MSCP_READ_LENGTH 32u

#define MSCP_CF_RPL 0x8000u
#define MSCP_CONTROLLER_HOST_FLAGS 0x00f0u
#define MSCP_CONTROLLER_CLASS 1u
#define MSCP_LF_SEQUENCE_RESET 0x0001u
#define MSCP_LF_FORMAT_CONTROLLER_ERROR 0u
#define MSCP_LF_EVENT_CONTROLLER_ERROR 10u

#define MSCP_MD_MBI 0x0002u
#define MSCP_MD_NXUNT 0x0001u
#define MSCP_MD_ONL_RIP 0x0001u
#define MSCP_MD_ONL_IGNMF 0x0002u
#define MSCP_MD_STWRP 0x0004u
#define MSCP_MD_HISLO 0x0008u
#define MSCP_MD_SUPWL 0x0010u
#define MSCP_MD_AVL_SPNDW 0x0001u
#define MSCP_MD_AVL_ALLCD 0x0002u
#define MSCP_MD_EXCAC 0x0020u
#define MSCP_MD_REUSE 0x0080u
#define MSCP_MD_SEREC 0x0100u
#define MSCP_MD_SECOR 0x0200u
#define MSCP_MD_ERROR 0x1000u
#define MSCP_MD_CLSEX 0x2000u
#define MSCP_MD_COMP 0x4000u
#define MSCP_MD_EXPRS 0x8000u

#define MSCP_ABORT_LENGTH 16u
#define MSCP_PAGE_TABLE_ENTRY_BYTES 4u

#define MSCP_GUS_MODIFIERS (MSCP_MD_NXUNT | MSCP_MD_CLSEX)
#define MSCP_AVAILABLE_MODIFIERS                                               \
    (MSCP_MD_AVL_SPNDW | MSCP_MD_AVL_ALLCD | MSCP_MD_EXCAC | MSCP_MD_CLSEX)
#define MSCP_ONL_MODIFIERS                                                     \
    (MSCP_MD_ONL_RIP | MSCP_MD_ONL_IGNMF | MSCP_MD_STWRP | MSCP_MD_EXCAC |     \
     MSCP_MD_CLSEX)
#define MSCP_SETUNIT_MODIFIERS (MSCP_MD_STWRP | MSCP_MD_EXCAC | MSCP_MD_CLSEX)

#define MSCP_UID_CLASS_DISK 2u
#define MSCP_UF_CMPRD 0x0001u
#define MSCP_UF_CMPWR 0x0002u
#define MSCP_UF_WRTPS 0x1000u
#define MSCP_HOST_SETTABLE_UNIT_FLAGS (MSCP_UF_CMPRD | MSCP_UF_CMPWR)

#define MSCP_SCC_FLAGS 9u
#define MSCP_SCC_TIMEOUT 10u
#define MSCP_SCC_VERSION 11u
#define MSCP_SCC_IDA 12u
#define MSCP_SCC_IDB 13u
#define MSCP_SCC_IDC 14u
#define MSCP_SCC_IDD 15u
#define MSCP_SCC_MAX_BYTE_COUNT_LOW 16u
#define MSCP_SCC_MAX_BYTE_COUNT_HIGH 17u

#define MSCP_LAST_FAIL_FORMAT_FLAGS 6u
#define MSCP_LAST_FAIL_EVENT 7u
#define MSCP_LAST_FAIL_CIDA 8u
#define MSCP_LAST_FAIL_CIDB 9u
#define MSCP_LAST_FAIL_CIDC 10u
#define MSCP_LAST_FAIL_CIDD 11u
#define MSCP_LAST_FAIL_VERSION 12u
#define MSCP_LAST_FAIL_ERROR 13u

#define MSCP_UNIT_MULTILUN 8u
#define MSCP_ONL_RESERVED_WORD 8u
#define MSCP_ONL_FLAGS 9u
#define MSCP_ONL_UIDA 12u
#define MSCP_ONL_UIDB 13u
#define MSCP_ONL_UIDC 14u
#define MSCP_ONL_UIDD 15u
#define MSCP_ONL_MEDIA_LOW 16u
#define MSCP_ONL_MEDIA_HIGH 17u
#define MSCP_UNIT_SHADOW_UNIT 18u
#define MSCP_UNIT_SHADOW_STATUS 19u
#define MSCP_ONL_SHADOW_RESERVED_LOW 18u
#define MSCP_ONL_SHADOW_RESERVED_WORDS 2u
#define MSCP_ONL_SIZE_LOW 20u
#define MSCP_ONL_SIZE_HIGH 21u

#define MSCP_GUS_TRACKS 20u
#define MSCP_GUS_GROUPS 21u
#define MSCP_GUS_CYLINDERS 22u
#define MSCP_GUS_UNIT_VERSION 23u

#define MSCP_GCS_CMD_STATUS_LOW 10u
#define MSCP_RW_BYTE_COUNT_LOW 8u
#define MSCP_RW_BUFFER_LOW 10u
#define MSCP_RW_MAPBASE_LOW 12u
#define MSCP_RW_LBN_LOW 16u
#define MSCP_SECTOR_BYTES 512u

typedef enum {
    MSCP_TRANSFER_ACCESS,
    MSCP_TRANSFER_COMPARE,
    MSCP_TRANSFER_ERASE,
    MSCP_TRANSFER_READ,
    MSCP_TRANSFER_WRITE,
} MSCP_TRANSFER;

static uint16_t transfer_modifier_mask(MSCP_TRANSFER transfer)
{
    switch (transfer) {
    case MSCP_TRANSFER_ACCESS:
    case MSCP_TRANSFER_COMPARE:
        return MSCP_MD_CLSEX | MSCP_MD_EXPRS | MSCP_MD_SEREC | MSCP_MD_SECOR;

    case MSCP_TRANSFER_ERASE:
        return MSCP_MD_CLSEX | MSCP_MD_EXPRS | MSCP_MD_SEREC | MSCP_MD_ERROR |
               MSCP_MD_HISLO | MSCP_MD_REUSE;

    case MSCP_TRANSFER_READ:
        return MSCP_MD_CLSEX | MSCP_MD_EXPRS | MSCP_MD_SEREC | MSCP_MD_SECOR |
               MSCP_MD_COMP;

    case MSCP_TRANSFER_WRITE:
        return MSCP_MD_CLSEX | MSCP_MD_EXPRS | MSCP_MD_SEREC | MSCP_MD_SECOR |
               MSCP_MD_ERROR | MSCP_MD_COMP | MSCP_MD_HISLO | MSCP_MD_REUSE |
               MSCP_MD_SUPWL;
    }
    return 0;
}

typedef struct {
    int32_t int_offset;
    uint32_t base;
    uint32_t length;
    uint32_t index;
} MSCP_RING;

enum {
    VAX_MSCP_STATE_STEP1,
    VAX_MSCP_STATE_STEP1_WRAP,
    VAX_MSCP_STATE_STEP2,
    VAX_MSCP_STATE_STEP3,
    VAX_MSCP_STATE_STEP3_PURGE_SA,
    VAX_MSCP_STATE_STEP3_PURGE_POLL,
    VAX_MSCP_STATE_STEP4,
    VAX_MSCP_STATE_UP,
    VAX_MSCP_STATE_DEAD,
};

static uint32_t init_queue_len(uint16_t s1, uint32_t shift)
{
    return 1u << ((s1 >> shift) & UQSSP_S1_Q_MASK);
}

static bool is_power_of_two(uint32_t value)
{
    return value != 0 && (value & (value - 1u)) == 0;
}

bool vax_mscp_profile_valid(const vax_mscp_profile *profile)
{
    if (profile == NULL)
        return false;
    if (profile->controller_model > 0xffu ||
        profile->controller_hw_version > 0xffu ||
        profile->controller_sw_version > 0xffu || profile->unit_model > 0xffu)
        return false;
    if (profile->max_units == 0 || profile->max_units > VAX_MSCP_MAX_UNITS)
        return false;
    if (profile->sectors_per_track == 0 || profile->tracks_per_group == 0 ||
        profile->groups_per_cylinder == 0)
        return false;
    if (!is_power_of_two(profile->buffer_map_bit) ||
        !is_power_of_two(profile->page_bytes))
        return false;
    if ((profile->mapped_descriptor_max & profile->buffer_map_bit) != 0)
        return false;
    if ((profile->unmapped_max_addr & profile->buffer_map_bit) != 0)
        return false;
    if ((profile->unmapped_max_addr & 1u) != 0)
        return false;
    if (profile->phys_addr_mask > profile->bus_addr_max)
        return false;
    if (profile->page_bytes < MSCP_PAGE_TABLE_ENTRY_BYTES ||
        (profile->page_bytes % MSCP_PAGE_TABLE_ENTRY_BYTES) != 0)
        return false;
    if (profile->phys_addr_mask == 0 || profile->bus_addr_max == 0 ||
        profile->pte_valid == 0 || profile->pte_pfn_mask == 0)
        return false;
    if (profile->forced_error_blocks > VAX_MSCP_FORCED_ERROR_MAX_BLOCKS)
        return false;
    return true;
}

static uint16_t max_units(const vax_mscp *ctlr)
{
    uint16_t units = ctlr->profile->max_units;

    if (units > VAX_MSCP_MAX_UNITS)
        return VAX_MSCP_MAX_UNITS;
    return units;
}

static uint32_t page_offset_mask(const vax_mscp *ctlr)
{
    return ctlr->profile->page_bytes - 1u;
}

static bool write_words(vax_mscp *ctlr, uint32_t addr, const uint16_t *buf,
                        uint32_t bytes)
{
    if (ctlr->bus.write_words == NULL)
        return false;
    return ctlr->bus.write_words(ctlr->bus.ctx, addr, buf, bytes) == 0;
}

static bool write_bytes(vax_mscp *ctlr, uint32_t addr, const uint8_t *buf,
                        uint32_t bytes)
{
    if (ctlr->bus.write_bytes != NULL)
        return ctlr->bus.write_bytes(ctlr->bus.ctx, addr, buf, bytes) == 0;

    if ((addr | bytes) & 1u)
        return false;
    for (uint32_t done = 0; done < bytes; done += 2) {
        uint16_t word = u16_from_u8_pair(buf[done], buf[done + 1u]);

        if (!write_words(ctlr, addr + done, &word, sizeof(word)))
            return false;
    }
    return true;
}

static bool read_sectors(vax_mscp *ctlr, uint16_t unit, uint32_t lbn,
                         uint8_t *buf, uint32_t sectors)
{
    if (ctlr->bus.read_sectors == NULL)
        return false;
    return ctlr->bus.read_sectors(ctlr->bus.ctx, unit, lbn, buf, sectors) == 0;
}

static bool write_sectors(vax_mscp *ctlr, uint16_t unit, uint32_t lbn,
                          const uint8_t *buf, uint32_t sectors)
{
    if (ctlr->bus.write_sectors == NULL)
        return false;
    return ctlr->bus.write_sectors(ctlr->bus.ctx, unit, lbn, buf, sectors) == 0;
}

static bool forced_error_settable(const vax_mscp *ctlr, uint32_t lbn)
{
    uint32_t blocks = ctlr->profile->forced_error_blocks;

    if (blocks > VAX_MSCP_FORCED_ERROR_MAX_BLOCKS)
        blocks = VAX_MSCP_FORCED_ERROR_MAX_BLOCKS;
    return lbn < blocks;
}

static bool sector_forced_error(const vax_mscp *ctlr, uint16_t unit,
                                uint32_t lbn)
{
    uint8_t bit;

    if (!forced_error_settable(ctlr, lbn))
        return false;
    bit = (uint8_t)(1u << (lbn & 7u));
    return (ctlr->unit[unit].forced_error[lbn >> 3] & bit) != 0;
}

static void set_sector_forced_error(vax_mscp *ctlr, uint16_t unit, uint32_t lbn,
                                    bool forced_error)
{
    uint8_t bit;

    if (!forced_error_settable(ctlr, lbn))
        return;
    bit = (uint8_t)(1u << (lbn & 7u));
    if (forced_error)
        ctlr->unit[unit].forced_error[lbn >> 3] |= bit;
    else
        ctlr->unit[unit].forced_error[lbn >> 3] &= (uint8_t)~bit;
}

static bool read_words(vax_mscp *ctlr, uint32_t addr, uint16_t *buf,
                       uint32_t bytes)
{
    if (ctlr->bus.read_words == NULL)
        return false;
    return ctlr->bus.read_words(ctlr->bus.ctx, addr, buf, bytes) == 0;
}

static bool read_bytes(vax_mscp *ctlr, uint32_t addr, uint8_t *buf,
                       uint32_t bytes)
{
    if (ctlr->bus.read_bytes != NULL)
        return ctlr->bus.read_bytes(ctlr->bus.ctx, addr, buf, bytes) == 0;

    if ((addr | bytes) & 1u)
        return false;
    for (uint32_t done = 0; done < bytes; done += 2) {
        uint16_t word;

        if (!read_words(ctlr, addr + done, &word, sizeof(word)))
            return false;
        buf[done] = word & 0xffu;
        buf[done + 1] = word >> 8;
    }
    return true;
}

static void fatal(vax_mscp *ctlr, uint16_t err)
{
    ctlr->sa = UQSSP_ERR | err;
    ctlr->state = VAX_MSCP_STATE_DEAD;
    ctlr->last_fail_code = err;
    ctlr->last_fail_valid = 1;
    ctlr->last_fail_pending = 0;
}

static void enter_step4(vax_mscp *ctlr)
{
    uint16_t zero[32];
    uint32_t rq_bytes = init_queue_len(ctlr->s1, UQSSP_S1_RQ_SHIFT) * 4u;
    uint32_t cq_bytes = init_queue_len(ctlr->s1, UQSSP_S1_CQ_SHIFT) * 4u;
    uint32_t base = ctlr->comm + (ctlr->purge_interrupt ? UQSSP_COMM_QQ_OFFSET
                                                        : UQSSP_COMM_CI_OFFSET);
    uint32_t end = ctlr->comm + rq_bytes + cq_bytes;
    uint32_t bytes = end - base;

    memset(zero, 0, sizeof(zero));
    for (uint32_t done = 0; done < bytes;) {
        uint32_t chunk = bytes - done;

        if (chunk > sizeof(zero))
            chunk = sizeof(zero);
        if (!write_words(ctlr, base + done, zero, chunk)) {
            fatal(ctlr, UQSSP_FATAL_QUEUE_WRITE);
            return;
        }
        done += chunk;
    }

    ctlr->sa = UQSSP_STEP4 | (ctlr->profile->controller_model << 4) |
               UQSSP_PORT_VERSION;
    ctlr->state = VAX_MSCP_STATE_STEP4;
}

static uint32_t desc_from_words(const uint16_t words[2])
{
    return u32_from_u16_pair(words[0], words[1]);
}

static void words_from_desc(uint16_t words[2], uint32_t desc)
{
    words[0] = (uint16_t)u32_low_u16(desc);
    words[1] = (uint16_t)u32_high_u16(desc);
}

static uint32_t phys_addr(const vax_mscp *ctlr, uint32_t addr)
{
    return addr & ctlr->profile->phys_addr_mask;
}

static uint32_t desc_packet_addr(const vax_mscp *ctlr, uint32_t desc)
{
    return desc & (ctlr->profile->phys_addr_mask & ~1u);
}

static bool read_desc(vax_mscp *ctlr, MSCP_RING *ring, uint32_t *desc)
{
    uint16_t words[2];

    if (!read_words(ctlr, ring->base + ring->index, words, sizeof(words))) {
        fatal(ctlr, UQSSP_FATAL_QUEUE_READ);
        return false;
    }
    *desc = desc_from_words(words);
    return true;
}

static bool release_desc(vax_mscp *ctlr, MSCP_RING *ring, uint32_t desc)
{
    uint16_t words[2];
    uint32_t released = desc_packet_addr(ctlr, desc) | UQ_DESC_FLAG;

    words_from_desc(words, released);
    if (!write_words(ctlr, ring->base + ring->index, words, sizeof(words))) {
        fatal(ctlr, UQSSP_FATAL_QUEUE_WRITE);
        return false;
    }
    ring->index = (ring->index + 4u) & (ring->length - 1u);
    return true;
}

static bool post_ring_interrupt(vax_mscp *ctlr, const MSCP_RING *ring,
                                bool request_interrupt)
{
    const uint16_t flag = 1;

    if (!request_interrupt)
        return true;
    if (!write_words(ctlr, ctlr->comm + ring->int_offset, &flag,
                     sizeof(flag))) {
        fatal(ctlr, UQSSP_FATAL_INTERRUPT_WRITE);
        return false;
    }
    if ((ctlr->profile->interrupt_vector_external ||
         ctlr->interrupt_vector != 0) &&
        (ctlr->bus.ring_interrupt != NULL))
        ctlr->bus.ring_interrupt(ctlr->bus.ctx);
    return true;
}

static bool ring_is_full(vax_mscp *ctlr, const MSCP_RING *ring, bool *is_full)
{
    MSCP_RING slot = *ring;

    *is_full = false;
    for (uint32_t offset = 0; offset < ring->length; offset += 4u) {
        uint32_t desc;

        slot.index = offset;
        if (!read_desc(ctlr, &slot, &desc))
            return false;
        if ((desc & UQ_DESC_OWN) == 0)
            return true;
    }
    *is_full = true;
    return true;
}

static void post_init_interrupt(vax_mscp *ctlr)
{
    if ((ctlr->s1 & UQSSP_S1_IE) &&
        (ctlr->profile->interrupt_vector_external ||
         ctlr->interrupt_vector != 0) &&
        (ctlr->bus.ring_interrupt != NULL))
        ctlr->bus.ring_interrupt(ctlr->bus.ctx);
}

static bool post_last_fail_packet(vax_mscp *ctlr)
{
    MSCP_RING rsp = {
        .int_offset = UQSSP_COMM_RI_OFFSET,
        .base = ctlr->comm,
        .length = init_queue_len(ctlr->s1, UQSSP_S1_RQ_SHIFT) * 4u,
        .index = ctlr->rsp_index,
    };
    uint16_t packet[UQ_PACKET_WORDS];
    uint32_t rsp_desc;
    bool rsp_was_empty;

    if (!read_desc(ctlr, &rsp, &rsp_desc))
        return false;
    if ((rsp_desc & UQ_DESC_OWN) == 0)
        return true;
    if (!ring_is_full(ctlr, &rsp, &rsp_was_empty))
        return false;

    memset(packet, 0, sizeof(packet));
    packet[UQ_WORD_LENGTH] = MSCP_LAST_FAIL_LENGTH;
    packet[UQ_WORD_CREDITS_TYPE] = UQ_TYPE_DATAGRAM << 4;
    packet[MSCP_LAST_FAIL_FORMAT_FLAGS] =
        (MSCP_LF_SEQUENCE_RESET << 8) | MSCP_LF_FORMAT_CONTROLLER_ERROR;
    packet[MSCP_LAST_FAIL_EVENT] = MSCP_LF_EVENT_CONTROLLER_ERROR;
    packet[MSCP_LAST_FAIL_CIDA] = 0;
    packet[MSCP_LAST_FAIL_CIDB] = 0;
    packet[MSCP_LAST_FAIL_CIDC] = 0;
    packet[MSCP_LAST_FAIL_CIDD] =
        (MSCP_CONTROLLER_CLASS << 8) | ctlr->profile->controller_model;
    packet[MSCP_LAST_FAIL_VERSION] =
        (ctlr->profile->controller_hw_version << 8) |
        ctlr->profile->controller_sw_version;
    packet[MSCP_LAST_FAIL_ERROR] = ctlr->last_fail_code;

    if (!write_words(ctlr, desc_packet_addr(ctlr, rsp_desc) - UQ_HDR_OFF,
                     packet, MSCP_LAST_FAIL_LENGTH + UQ_HDR_OFF)) {
        fatal(ctlr, UQSSP_FATAL_PACKET_WRITE);
        return false;
    }
    if (!release_desc(ctlr, &rsp, rsp_desc))
        return false;
    ctlr->rsp_index = rsp.index;
    ctlr->last_fail_valid = 0;
    ctlr->last_fail_pending = 0;
    (void)post_ring_interrupt(
        ctlr, &rsp, rsp_was_empty && ((rsp_desc & UQ_DESC_FLAG) != 0));
    return true;
}

static void finish_packet(uint16_t packet[UQ_PACKET_WORDS], uint16_t opcode,
                          uint16_t status, uint16_t length, uint16_t credits)
{
    packet[UQ_WORD_OPCODE] = opcode | MSCP_OP_END;
    packet[UQ_WORD_STATUS] = status;
    packet[UQ_WORD_LENGTH] = length;
    packet[UQ_WORD_CREDITS_TYPE] = (UQ_TYPE_SEQ << 4) | credits;
}

static void finish_invalid_command(uint16_t packet[UQ_PACKET_WORDS],
                                   uint16_t status)
{
    finish_packet(packet, 0, status, 12, MSCP_RESPONSE_CREDITS);
}

static void finish_invalid_opcode(uint16_t packet[UQ_PACKET_WORDS])
{
    finish_invalid_command(packet, MSCP_STATUS_INVALID_OPCODE);
}

static uint16_t command_min_length(uint16_t opcode)
{
    switch (opcode) {
    case MSCP_OP_ABORT:
    case MSCP_OP_GCS:
        return MSCP_ABORT_LENGTH;

    case MSCP_OP_GUS:
    case MSCP_OP_AVAILABLE:
    case MSCP_OP_DAP:
    case MSCP_OP_NOP17:
    case MSCP_OP_NOP19:
        return MSCP_AVAILABLE_LENGTH;

    case MSCP_OP_SCC:
        return MSCP_SCC_LENGTH;

    case MSCP_OP_DISPLAY:
        return MSCP_MIN_DISPLAY_LENGTH;

    case MSCP_OP_ONL:
    case MSCP_OP_SETUNITC:
        return MSCP_MIN_ONL_LENGTH;

    case MSCP_OP_ACCESS:
    case MSCP_OP_ERASE:
    case MSCP_OP_COMPARE:
    case MSCP_OP_READ:
    case MSCP_OP_WRITE:
        return MSCP_MIN_RW_LENGTH;

    default:
        return 0;
    }
}

static void finish_transfer_packet(uint16_t packet[UQ_PACKET_WORDS],
                                   uint16_t opcode, uint16_t status)
{
    if (status == MSCP_STATUS_INVALID_MODIFIER ||
        status == MSCP_STATUS_INVALID_BUFFER_RESERVED_FIELD)
        finish_invalid_command(packet, status);
    else
        finish_packet(packet, opcode, status, MSCP_READ_LENGTH,
                      MSCP_RESPONSE_CREDITS);
}

static bool invalid_modifier(const uint16_t packet[UQ_PACKET_WORDS],
                             uint16_t valid)
{
    return (packet[UQ_WORD_STATUS] & ~valid) != 0;
}

static uint16_t packet_connection_id(const uint16_t packet[UQ_PACKET_WORDS])
{
    return packet[UQ_WORD_CREDITS_TYPE] >> 8;
}

static uint16_t packet_type(const uint16_t packet[UQ_PACKET_WORDS])
{
    return (packet[UQ_WORD_CREDITS_TYPE] >> 4) & 0xfu;
}

static uint16_t
invalid_onl_suc_reserved_field(const uint16_t packet[UQ_PACKET_WORDS])
{
    if (packet[MSCP_ONL_RESERVED_WORD] != 0)
        return MSCP_STATUS_INVALID_ONL_RESERVED_FIELD;

    for (uint32_t i = MSCP_ONL_SHADOW_RESERVED_LOW;
         i < MSCP_ONL_SHADOW_RESERVED_LOW + MSCP_ONL_SHADOW_RESERVED_WORDS;
         i++) {
        if (packet[i] != 0)
            return MSCP_STATUS_INVALID_SHADOW_RESERVED_FIELD;
    }

    return MSCP_STATUS_SUCCESS;
}

static void finish_display(uint16_t packet[UQ_PACKET_WORDS])
{
    uint16_t modifiers = packet[UQ_WORD_STATUS];

    if (invalid_modifier(packet, MSCP_MD_MBI | MSCP_MD_CLSEX)) {
        finish_invalid_command(packet, MSCP_STATUS_INVALID_MODIFIER);
    } else if ((modifiers & MSCP_MD_MBI) != 0) {
        finish_packet(packet, MSCP_OP_DISPLAY, MSCP_STATUS_INVALID_ITEM, 12,
                      MSCP_RESPONSE_CREDITS);
    } else {
        finish_packet(packet, MSCP_OP_DISPLAY, MSCP_STATUS_SUCCESS, 12,
                      MSCP_RESPONSE_CREDITS);
    }
}

static void put_u32(uint16_t packet[UQ_PACKET_WORDS], uint32_t index,
                    uint32_t value)
{
    packet[index] = (uint16_t)u32_low_u16(value);
    packet[index + 1] = (uint16_t)u32_high_u16(value);
}

static uint32_t get_u32(const uint16_t packet[UQ_PACKET_WORDS], uint32_t index)
{
    return u32_from_u16_pair(packet[index], packet[index + 1]);
}

static uint16_t get_status_unit(vax_mscp *ctlr,
                                const uint16_t packet[UQ_PACKET_WORDS])
{
    uint16_t requested = packet[UQ_WORD_UNIT];

    if ((packet[UQ_WORD_STATUS] & MSCP_GUS_NEXT_UNIT) == 0)
        return requested;

    for (uint16_t unit = requested; unit < max_units(ctlr); unit++) {
        if (ctlr->unit[unit].present)
            return unit;
    }
    return 0;
}

static void apply_unit_flags(vax_mscp *ctlr, uint16_t packet[UQ_PACKET_WORDS],
                             uint16_t unit)
{
    uint16_t flags = packet[MSCP_ONL_FLAGS] & MSCP_HOST_SETTABLE_UNIT_FLAGS;

    ctlr->unit[unit].compare_reads = (flags & MSCP_UF_CMPRD) != 0;
    ctlr->unit[unit].compare_writes = (flags & MSCP_UF_CMPWR) != 0;
    if ((packet[UQ_WORD_STATUS] & MSCP_MD_STWRP) != 0)
        ctlr->unit[unit].write_protected =
            (packet[MSCP_ONL_FLAGS] & MSCP_UF_WRTPS) != 0;
}

static void put_unit_status(vax_mscp *ctlr, uint16_t packet[UQ_PACKET_WORDS],
                            uint16_t unit, bool include_size)
{
    uint32_t blocks = ctlr->unit[unit].blocks;

    memset(&packet[8], 0, (UQ_PACKET_WORDS - 8u) * sizeof(packet[0]));
    packet[MSCP_UNIT_MULTILUN] = unit;
    packet[MSCP_ONL_FLAGS] = 0x8000u;
    if (ctlr->unit[unit].compare_reads)
        packet[MSCP_ONL_FLAGS] |= MSCP_UF_CMPRD;
    if (ctlr->unit[unit].compare_writes)
        packet[MSCP_ONL_FLAGS] |= MSCP_UF_CMPWR;
    if (ctlr->unit[unit].write_protected)
        packet[MSCP_ONL_FLAGS] |= MSCP_UF_WRTPS;
    packet[MSCP_ONL_UIDA] = unit;
    packet[MSCP_ONL_UIDB] = 0;
    packet[MSCP_ONL_UIDC] = 0;
    packet[MSCP_ONL_UIDD] =
        (MSCP_UID_CLASS_DISK << 8) | ctlr->profile->unit_model;
    put_u32(packet, MSCP_ONL_MEDIA_LOW, ctlr->profile->media_id);
    packet[MSCP_UNIT_SHADOW_UNIT] = unit;
    packet[MSCP_UNIT_SHADOW_STATUS] = 0;
    if (include_size) {
        put_u32(packet, MSCP_ONL_SIZE_LOW, blocks);
    } else {
        packet[MSCP_GUS_TRACKS] = ctlr->profile->sectors_per_track;
        packet[MSCP_GUS_GROUPS] = ctlr->profile->tracks_per_group;
        packet[MSCP_GUS_CYLINDERS] = ctlr->profile->groups_per_cylinder;
        packet[MSCP_GUS_UNIT_VERSION] = ctlr->profile->unit_version;
    }
}

static void put_controller_status(vax_mscp *ctlr,
                                  uint16_t packet[UQ_PACKET_WORDS])
{
    packet[MSCP_SCC_FLAGS] =
        (packet[MSCP_SCC_FLAGS] & MSCP_CONTROLLER_HOST_FLAGS) | MSCP_CF_RPL;
    packet[MSCP_SCC_TIMEOUT] = ctlr->profile->controller_timeout;
    packet[MSCP_SCC_VERSION] = (ctlr->profile->controller_hw_version << 8) |
                               ctlr->profile->controller_sw_version;
    packet[MSCP_SCC_IDA] = 0;
    packet[MSCP_SCC_IDB] = 0;
    packet[MSCP_SCC_IDC] = 0;
    packet[MSCP_SCC_IDD] =
        (MSCP_CONTROLLER_CLASS << 8) | ctlr->profile->controller_model;
    packet[MSCP_SCC_MAX_BYTE_COUNT_LOW] = 0;
    packet[MSCP_SCC_MAX_BYTE_COUNT_HIGH] = 0;
}

static bool read_u32(vax_mscp *ctlr, uint32_t addr, uint32_t *value)
{
    uint16_t words[2];

    if (!read_words(ctlr, addr, words, sizeof(words)))
        return false;
    *value = desc_from_words(words);
    return true;
}

static bool read_map_pte(vax_mscp *ctlr, uint32_t mapbase, uint32_t page_index,
                         uint32_t *pte)
{
    uint64_t addr = (uint64_t)mapbase +
                    ((uint64_t)page_index * MSCP_PAGE_TABLE_ENTRY_BYTES);

    if (addr + sizeof(uint32_t) - 1u > ctlr->profile->bus_addr_max)
        return false;
    if (ctlr->bus.read_map_pte != NULL)
        return ctlr->bus.read_map_pte(ctlr->bus.ctx, mapbase, page_index,
                                      pte) == 0;
    return read_u32(ctlr, (uint32_t)addr, pte);
}

static uint16_t write_mapped_buffer(vax_mscp *ctlr, uint32_t buffer,
                                    uint32_t mapbase, uint32_t offset,
                                    const uint8_t *data, uint32_t bytes)
{
    uint32_t current = (buffer & ~ctlr->profile->buffer_map_bit) + offset;
    uint32_t done = 0;

    while (done < bytes) {
        uint32_t pte;
        uint32_t page_offset = current & page_offset_mask(ctlr);
        uint32_t page_index = current / ctlr->profile->page_bytes;
        uint32_t chunk = ctlr->profile->page_bytes - page_offset;
        uint32_t phys;

        if ((current & 1u) != 0)
            return MSCP_STATUS_HOST_ODD_ADDRESS;
        if (chunk > bytes - done)
            chunk = bytes - done;
        if (!read_map_pte(ctlr, mapbase, page_index, &pte))
            return MSCP_STATUS_HOST_ERROR;
        if ((pte & ctlr->profile->pte_valid) == 0)
            return MSCP_STATUS_HOST_INVALID_PTE;
        phys =
            ((pte & ctlr->profile->pte_pfn_mask) * ctlr->profile->page_bytes) +
            page_offset;
        if (!write_bytes(ctlr, phys, &data[done], chunk))
            return MSCP_STATUS_HOST_ERROR;
        current += chunk;
        done += chunk;
    }

    return MSCP_STATUS_SUCCESS;
}

static uint16_t read_mapped_buffer(vax_mscp *ctlr, uint32_t buffer,
                                   uint32_t mapbase, uint32_t offset,
                                   uint8_t *data, uint32_t bytes)
{
    uint32_t current = (buffer & ~ctlr->profile->buffer_map_bit) + offset;
    uint32_t done = 0;

    while (done < bytes) {
        uint32_t pte;
        uint32_t page_offset = current & page_offset_mask(ctlr);
        uint32_t page_index = current / ctlr->profile->page_bytes;
        uint32_t chunk = ctlr->profile->page_bytes - page_offset;
        uint32_t phys;

        if ((current & 1u) != 0)
            return MSCP_STATUS_HOST_ODD_ADDRESS;
        if (chunk > bytes - done)
            chunk = bytes - done;
        if (!read_map_pte(ctlr, mapbase, page_index, &pte))
            return MSCP_STATUS_HOST_ERROR;
        if ((pte & ctlr->profile->pte_valid) == 0)
            return MSCP_STATUS_HOST_INVALID_PTE;
        phys =
            ((pte & ctlr->profile->pte_pfn_mask) * ctlr->profile->page_bytes) +
            page_offset;
        if (!read_bytes(ctlr, phys, &data[done], chunk))
            return MSCP_STATUS_HOST_ERROR;
        current += chunk;
        done += chunk;
    }

    return MSCP_STATUS_SUCCESS;
}

static bool valid_unmapped_bi_buffer(vax_mscp *ctlr, uint32_t buffer,
                                     uint32_t offset, uint32_t bytes)
{
    uint64_t start = (uint64_t)buffer + offset;
    uint64_t last = start + bytes - 1u;

    return last <= ctlr->profile->unmapped_max_addr;
}

static bool valid_mapped_pte_range(vax_mscp *ctlr, uint32_t buffer,
                                   uint32_t mapbase, uint32_t bytes)
{
    uint64_t start = buffer & ~ctlr->profile->buffer_map_bit;
    uint64_t last = start + bytes - 1u;
    uint64_t last_page;
    uint64_t pte_addr;

    if (last > ctlr->profile->mapped_descriptor_max)
        return false;
    last_page = last / ctlr->profile->page_bytes;
    pte_addr = (uint64_t)mapbase + (last_page * MSCP_PAGE_TABLE_ENTRY_BYTES);
    return pte_addr + sizeof(uint32_t) - 1u <= ctlr->profile->bus_addr_max;
}

static bool transfer_uses_host_buffer(MSCP_TRANSFER transfer)
{
    return transfer == MSCP_TRANSFER_COMPARE ||
           transfer == MSCP_TRANSFER_READ || transfer == MSCP_TRANSFER_WRITE;
}

static bool reserved_transfer_buffer(const uint16_t packet[UQ_PACKET_WORDS])
{
    for (uint32_t i = MSCP_RW_BUFFER_LOW; i < MSCP_RW_BUFFER_LOW + 6u; i++) {
        if (packet[i] != 0)
            return true;
    }
    return false;
}

static uint16_t validate_initial_data_buffer(vax_mscp *ctlr, uint32_t buffer,
                                             uint32_t mapbase, uint32_t bytes)
{
    if ((buffer & ctlr->profile->buffer_map_bit) == 0) {
        if ((buffer & 1u) != 0)
            return MSCP_STATUS_HOST_ODD_ADDRESS;
        if (!valid_unmapped_bi_buffer(ctlr, buffer, 0, bytes))
            return MSCP_STATUS_HOST_ERROR;
        return MSCP_STATUS_SUCCESS;
    }

    if ((buffer & 1u) != 0)
        return MSCP_STATUS_HOST_ODD_ADDRESS;
    if ((mapbase & ctlr->profile->mapped_mapbase_invalid) != 0)
        return MSCP_STATUS_HOST_ERROR;
    if (!valid_mapped_pte_range(ctlr, buffer, mapbase, bytes))
        return MSCP_STATUS_HOST_ERROR;
    return MSCP_STATUS_SUCCESS;
}

static uint16_t write_data_buffer(vax_mscp *ctlr, uint32_t buffer,
                                  uint32_t mapbase, uint32_t offset,
                                  const uint8_t *data, uint32_t bytes)
{
    if ((buffer & ctlr->profile->buffer_map_bit) == 0) {
        if (((buffer + offset) & 1u) != 0)
            return MSCP_STATUS_HOST_ODD_ADDRESS;
        if (!valid_unmapped_bi_buffer(ctlr, buffer, offset, bytes))
            return MSCP_STATUS_HOST_ERROR;
        return write_bytes(ctlr, buffer + offset, data, bytes)
                   ? MSCP_STATUS_SUCCESS
                   : MSCP_STATUS_HOST_ERROR;
    }
    return write_mapped_buffer(ctlr, buffer, mapbase, offset, data, bytes);
}

static uint16_t read_data_buffer(vax_mscp *ctlr, uint32_t buffer,
                                 uint32_t mapbase, uint32_t offset,
                                 uint8_t *data, uint32_t bytes)
{
    if ((buffer & ctlr->profile->buffer_map_bit) == 0) {
        if (((buffer + offset) & 1u) != 0)
            return MSCP_STATUS_HOST_ODD_ADDRESS;
        if (!valid_unmapped_bi_buffer(ctlr, buffer, offset, bytes))
            return MSCP_STATUS_HOST_ERROR;
        return read_bytes(ctlr, buffer + offset, data, bytes)
                   ? MSCP_STATUS_SUCCESS
                   : MSCP_STATUS_HOST_ERROR;
    }
    return read_mapped_buffer(ctlr, buffer, mapbase, offset, data, bytes);
}

static uint16_t do_transfer(vax_mscp *ctlr, uint16_t packet[UQ_PACKET_WORDS],
                            MSCP_TRANSFER transfer)
{
    uint16_t unit = packet[UQ_WORD_UNIT];
    uint32_t bytes = get_u32(packet, MSCP_RW_BYTE_COUNT_LOW);
    uint32_t buffer = get_u32(packet, MSCP_RW_BUFFER_LOW);
    uint32_t mapbase = get_u32(packet, MSCP_RW_MAPBASE_LOW);
    uint32_t lbn = get_u32(packet, MSCP_RW_LBN_LOW);
    uint16_t modifiers = packet[UQ_WORD_STATUS];
    uint8_t sector[MSCP_SECTOR_BYTES];
    uint8_t host[MSCP_SECTOR_BYTES];
    uint8_t verify[MSCP_SECTOR_BYTES];
    uint16_t status;
    uint32_t sectors;
    bool compare_reads;
    bool compare_writes;

    if ((unit >= max_units(ctlr)) || !ctlr->unit[unit].present)
        return MSCP_STATUS_OFFLINE;
    if (!ctlr->unit[unit].online)
        return MSCP_STATUS_AVAILABLE;
    compare_reads =
        ((modifiers & MSCP_MD_COMP) != 0) || ctlr->unit[unit].compare_reads;
    compare_writes =
        ((modifiers & MSCP_MD_COMP) != 0) || ctlr->unit[unit].compare_writes;
    if ((modifiers & ~transfer_modifier_mask(transfer)) != 0)
        return MSCP_STATUS_INVALID_MODIFIER;
    if ((transfer == MSCP_TRANSFER_ERASE || transfer == MSCP_TRANSFER_WRITE) &&
        (modifiers & (MSCP_MD_HISLO | MSCP_MD_REUSE)) != 0)
        return MSCP_STATUS_INVALID_MODIFIER;
    if ((transfer == MSCP_TRANSFER_ACCESS || transfer == MSCP_TRANSFER_ERASE) &&
        reserved_transfer_buffer(packet))
        return MSCP_STATUS_INVALID_BUFFER_RESERVED_FIELD;
    if ((transfer == MSCP_TRANSFER_ERASE || transfer == MSCP_TRANSFER_WRITE) &&
        ctlr->unit[unit].write_protected)
        return MSCP_STATUS_WRITE_PROTECTED_SOFTWARE;
    if (bytes == 0)
        return MSCP_STATUS_INVALID_BYTE_COUNT;
    sectors = (bytes + (MSCP_SECTOR_BYTES - 1u)) / MSCP_SECTOR_BYTES;
    if (lbn >= ctlr->unit[unit].blocks)
        return MSCP_STATUS_INVALID_LBN;
    if (sectors > ctlr->unit[unit].blocks - lbn) {
        put_u32(packet, MSCP_RW_BYTE_COUNT_LOW, 0);
        return MSCP_STATUS_INVALID_BYTE_COUNT;
    }
    if ((bytes & 1u) != 0) {
        put_u32(packet, MSCP_RW_BYTE_COUNT_LOW, 0);
        return MSCP_STATUS_HOST_ODD_BYTE_COUNT;
    }
    if (transfer_uses_host_buffer(transfer)) {
        status = validate_initial_data_buffer(ctlr, buffer, mapbase, bytes);
        if (status != MSCP_STATUS_SUCCESS) {
            put_u32(packet, MSCP_RW_BYTE_COUNT_LOW, 0);
            return status;
        }
    }

    for (uint32_t done = 0; done < bytes;) {
        uint32_t chunk = bytes - done;
        uint32_t block = lbn + (done / MSCP_SECTOR_BYTES);
        bool forced_error;

        if (chunk > MSCP_SECTOR_BYTES)
            chunk = MSCP_SECTOR_BYTES;
        if (!read_sectors(ctlr, unit, block, sector, 1))
            return MSCP_STATUS_HOST_ERROR;
        forced_error = sector_forced_error(ctlr, unit, block);

        switch (transfer) {
        case MSCP_TRANSFER_ACCESS:
            if (forced_error) {
                put_u32(packet, MSCP_RW_BYTE_COUNT_LOW, done);
                return MSCP_STATUS_DATA_ERROR_FORCE_ERROR;
            }
            break;

        case MSCP_TRANSFER_COMPARE:
            if (forced_error) {
                put_u32(packet, MSCP_RW_BYTE_COUNT_LOW, done);
                return MSCP_STATUS_DATA_ERROR_FORCE_ERROR;
            }
            status = read_data_buffer(ctlr, buffer, mapbase, done, host, chunk);
            if (status != MSCP_STATUS_SUCCESS) {
                put_u32(packet, MSCP_RW_BYTE_COUNT_LOW, done);
                return status;
            }
            if (memcmp(sector, host, chunk) != 0) {
                put_u32(packet, MSCP_RW_BYTE_COUNT_LOW, done);
                return MSCP_STATUS_COMPARE_ERROR;
            }
            break;

        case MSCP_TRANSFER_ERASE:
            memset(sector, 0, chunk);
            if (!write_sectors(ctlr, unit, block, sector, 1))
                return MSCP_STATUS_HOST_ERROR;
            set_sector_forced_error(ctlr, unit, block,
                                    (modifiers & MSCP_MD_ERROR) != 0);
            break;

        case MSCP_TRANSFER_READ:
            status =
                write_data_buffer(ctlr, buffer, mapbase, done, sector, chunk);
            if (status != MSCP_STATUS_SUCCESS) {
                put_u32(packet, MSCP_RW_BYTE_COUNT_LOW, done);
                return status;
            }
            if (forced_error) {
                put_u32(packet, MSCP_RW_BYTE_COUNT_LOW, done);
                return MSCP_STATUS_DATA_ERROR_FORCE_ERROR;
            }
            if (compare_reads) {
                status =
                    read_data_buffer(ctlr, buffer, mapbase, done, host, chunk);
                if (status != MSCP_STATUS_SUCCESS) {
                    put_u32(packet, MSCP_RW_BYTE_COUNT_LOW, done);
                    return status;
                }
                if (!read_sectors(ctlr, unit, lbn + (done / MSCP_SECTOR_BYTES),
                                  verify, 1))
                    return MSCP_STATUS_HOST_ERROR;
                if (memcmp(verify, host, chunk) != 0) {
                    put_u32(packet, MSCP_RW_BYTE_COUNT_LOW, done);
                    return MSCP_STATUS_COMPARE_ERROR;
                }
            }
            break;

        case MSCP_TRANSFER_WRITE:
            status =
                read_data_buffer(ctlr, buffer, mapbase, done, sector, chunk);
            if (status != MSCP_STATUS_SUCCESS) {
                put_u32(packet, MSCP_RW_BYTE_COUNT_LOW, done);
                return status;
            }
            if (!write_sectors(ctlr, unit, block, sector, 1))
                return MSCP_STATUS_HOST_ERROR;
            set_sector_forced_error(ctlr, unit, block,
                                    (modifiers & MSCP_MD_ERROR) != 0);
            if (compare_writes) {
                status =
                    read_data_buffer(ctlr, buffer, mapbase, done, host, chunk);
                if (status != MSCP_STATUS_SUCCESS) {
                    put_u32(packet, MSCP_RW_BYTE_COUNT_LOW, done);
                    return status;
                }
                if (!read_sectors(ctlr, unit, lbn + (done / MSCP_SECTOR_BYTES),
                                  verify, 1))
                    return MSCP_STATUS_HOST_ERROR;
                if (memcmp(verify, host, chunk) != 0) {
                    put_u32(packet, MSCP_RW_BYTE_COUNT_LOW, done);
                    return MSCP_STATUS_COMPARE_ERROR;
                }
                if ((modifiers & MSCP_MD_ERROR) != 0) {
                    put_u32(packet, MSCP_RW_BYTE_COUNT_LOW, done);
                    return MSCP_STATUS_DATA_ERROR_FORCE_ERROR;
                }
            }
            break;
        }
        done += chunk;
    }
    return MSCP_STATUS_SUCCESS;
}

static bool process_command(vax_mscp *ctlr)
{
    MSCP_RING rsp = {
        .int_offset = UQSSP_COMM_RI_OFFSET,
        .base = ctlr->comm,
        .length = init_queue_len(ctlr->s1, UQSSP_S1_RQ_SHIFT) * 4u,
        .index = ctlr->rsp_index,
    };
    MSCP_RING cmd = {
        .int_offset = UQSSP_COMM_CI_OFFSET,
        .base = ctlr->comm + rsp.length,
        .length = init_queue_len(ctlr->s1, UQSSP_S1_CQ_SHIFT) * 4u,
        .index = ctlr->cmd_index,
    };
    uint32_t cmd_desc;
    uint32_t rsp_desc;
    uint16_t packet[UQ_PACKET_WORDS];
    bool cmd_was_full;
    bool rsp_was_empty;
    uint16_t opcode;
    uint16_t min_length;

    if (!read_desc(ctlr, &cmd, &cmd_desc) || ((cmd_desc & UQ_DESC_OWN) == 0))
        return false;
    if (!read_desc(ctlr, &rsp, &rsp_desc) || ((rsp_desc & UQ_DESC_OWN) == 0))
        return false;
    if (!ring_is_full(ctlr, &cmd, &cmd_was_full))
        return false;
    if (!ring_is_full(ctlr, &rsp, &rsp_was_empty))
        return false;
    if (!read_words(ctlr, desc_packet_addr(ctlr, cmd_desc) - UQ_HDR_OFF, packet,
                    sizeof(packet))) {
        fatal(ctlr, UQSSP_FATAL_PACKET_READ);
        return false;
    }
    if (packet_type(packet) != UQ_TYPE_SEQ) {
        fatal(ctlr, UQSSP_FATAL_PROTOCOL_INCOMPATIBILITY);
        return false;
    }
    if (packet_connection_id(packet) != UQ_CONNECTION_ID_DISK) {
        fatal(ctlr, UQSSP_FATAL_INVALID_CONNECTION_ID);
        return false;
    }
    if (!release_desc(ctlr, &cmd, cmd_desc))
        return false;
    if (cmd_was_full &&
        !post_ring_interrupt(ctlr, &cmd, (cmd_desc & UQ_DESC_FLAG) != 0))
        return false;
    ctlr->cmd_index = cmd.index;

    opcode = packet[UQ_WORD_OPCODE] & 0xffu;
    min_length = command_min_length(opcode);
    if ((min_length != 0) && (packet[UQ_WORD_LENGTH] < min_length)) {
        finish_invalid_command(packet, MSCP_STATUS_INVALID_MESSAGE_LENGTH);
        goto command_complete;
    }
    if ((min_length != 0) && (packet[UQ_WORD_RESERVED] != 0)) {
        finish_invalid_command(packet, MSCP_STATUS_INVALID_RESERVED_FIELD);
        goto command_complete;
    }

    switch (opcode) {
    case MSCP_OP_ABORT:
        if (invalid_modifier(packet, 0))
            finish_invalid_command(packet, MSCP_STATUS_INVALID_MODIFIER);
        else
            finish_packet(packet, MSCP_OP_ABORT, MSCP_STATUS_SUCCESS,
                          MSCP_ABORT_LENGTH, MSCP_RESPONSE_CREDITS);
        break;
    case MSCP_OP_GCS:
        if (invalid_modifier(packet, 0)) {
            finish_invalid_command(packet, MSCP_STATUS_INVALID_MODIFIER);
        } else {
            put_u32(packet, MSCP_GCS_CMD_STATUS_LOW, 0);
            finish_packet(packet, MSCP_OP_GCS, MSCP_STATUS_SUCCESS,
                          MSCP_GCS_LENGTH, MSCP_RESPONSE_CREDITS);
        }
        break;
    case MSCP_OP_GUS: {
        uint16_t unit = get_status_unit(ctlr, packet);
        if (invalid_modifier(packet, MSCP_GUS_MODIFIERS)) {
            finish_invalid_command(packet, MSCP_STATUS_INVALID_MODIFIER);
        } else {
            packet[UQ_WORD_UNIT] = unit;
            if ((unit < max_units(ctlr)) && ctlr->unit[unit].present) {
                put_unit_status(ctlr, packet, unit, false);
                finish_packet(packet, MSCP_OP_GUS,
                              ctlr->unit[unit].online ? MSCP_STATUS_SUCCESS
                                                      : MSCP_STATUS_AVAILABLE,
                              MSCP_GUS_LENGTH, MSCP_RESPONSE_CREDITS);
            } else
                finish_packet(packet, MSCP_OP_GUS, MSCP_STATUS_OFFLINE,
                              MSCP_GUS_LENGTH, MSCP_RESPONSE_CREDITS);
        }
        break;
    }
    case MSCP_OP_SCC:
        if (invalid_modifier(packet, 0)) {
            finish_invalid_command(packet, MSCP_STATUS_INVALID_MODIFIER);
        } else if (packet[8] != 0) {
            finish_invalid_command(packet, MSCP_STATUS_INVALID_MSCP_VERSION);
        } else {
            put_controller_status(ctlr, packet);
            finish_packet(packet, MSCP_OP_SCC, MSCP_STATUS_SUCCESS,
                          MSCP_SCC_LENGTH, MSCP_SCC_CREDITS);
        }
        break;
    case MSCP_OP_DISPLAY:
        finish_display(packet);
        break;
    case MSCP_OP_DAP:
        if (invalid_modifier(packet, 0))
            finish_invalid_command(packet, MSCP_STATUS_INVALID_MODIFIER);
        else
            finish_packet(packet, MSCP_OP_DAP, MSCP_STATUS_SUCCESS, 12,
                          MSCP_RESPONSE_CREDITS);
        break;
    case MSCP_OP_AVAILABLE: {
        uint16_t unit = packet[UQ_WORD_UNIT];
        if (invalid_modifier(packet, MSCP_AVAILABLE_MODIFIERS)) {
            finish_invalid_command(packet, MSCP_STATUS_INVALID_MODIFIER);
        } else if ((unit < max_units(ctlr)) && ctlr->unit[unit].present) {
            ctlr->unit[unit].online = 0;
            finish_packet(packet, MSCP_OP_AVAILABLE, MSCP_STATUS_SUCCESS,
                          MSCP_AVAILABLE_LENGTH, MSCP_RESPONSE_CREDITS);
        } else
            finish_packet(packet, MSCP_OP_AVAILABLE, MSCP_STATUS_OFFLINE,
                          MSCP_AVAILABLE_LENGTH, MSCP_RESPONSE_CREDITS);
        break;
    }
    case MSCP_OP_ONL: {
        uint16_t unit = packet[UQ_WORD_UNIT];
        uint16_t reserved_status = invalid_onl_suc_reserved_field(packet);
        if (invalid_modifier(packet, MSCP_ONL_MODIFIERS)) {
            finish_invalid_command(packet, MSCP_STATUS_INVALID_MODIFIER);
        } else if (reserved_status != MSCP_STATUS_SUCCESS) {
            finish_invalid_command(packet, reserved_status);
        } else if ((unit < max_units(ctlr)) && ctlr->unit[unit].present) {
            if (ctlr->unit[unit].online) {
                put_unit_status(ctlr, packet, unit, true);
                finish_packet(packet, MSCP_OP_ONL,
                              MSCP_STATUS_SUCCESS_ALREADY_ONLINE,
                              MSCP_ONL_LENGTH, MSCP_RESPONSE_CREDITS);
                break;
            }
            ctlr->unit[unit].online = 1;
            apply_unit_flags(ctlr, packet, unit);
            put_unit_status(ctlr, packet, unit, true);
            finish_packet(packet, MSCP_OP_ONL, MSCP_STATUS_SUCCESS,
                          MSCP_ONL_LENGTH, MSCP_RESPONSE_CREDITS);
        } else
            finish_packet(packet, MSCP_OP_ONL, MSCP_STATUS_OFFLINE,
                          MSCP_ONL_LENGTH, MSCP_RESPONSE_CREDITS);
        break;
    }
    case MSCP_OP_SETUNITC: {
        uint16_t unit = packet[UQ_WORD_UNIT];
        uint16_t reserved_status = invalid_onl_suc_reserved_field(packet);
        if (invalid_modifier(packet, MSCP_SETUNIT_MODIFIERS)) {
            finish_invalid_command(packet, MSCP_STATUS_INVALID_MODIFIER);
        } else if (reserved_status != MSCP_STATUS_SUCCESS) {
            finish_invalid_command(packet, reserved_status);
        } else if ((unit < max_units(ctlr)) && ctlr->unit[unit].present) {
            if (ctlr->unit[unit].online)
                apply_unit_flags(ctlr, packet, unit);
            put_unit_status(ctlr, packet, unit, true);
            finish_packet(packet, MSCP_OP_SETUNITC,
                          ctlr->unit[unit].online ? MSCP_STATUS_SUCCESS
                                                  : MSCP_STATUS_AVAILABLE,
                          MSCP_ONL_LENGTH, MSCP_RESPONSE_CREDITS);
        } else
            finish_packet(packet, MSCP_OP_SETUNITC, MSCP_STATUS_OFFLINE,
                          MSCP_ONL_LENGTH, MSCP_RESPONSE_CREDITS);
        break;
    }
    case MSCP_OP_NOP17:
    case MSCP_OP_NOP19:
        finish_packet(packet, packet[UQ_WORD_OPCODE] & 0xffu,
                      MSCP_STATUS_SUCCESS, 12, MSCP_RESPONSE_CREDITS);
        break;
    case MSCP_OP_ACCESS:
        finish_transfer_packet(packet, MSCP_OP_ACCESS,
                               do_transfer(ctlr, packet, MSCP_TRANSFER_ACCESS));
        break;
    case MSCP_OP_COMPARE:
        finish_transfer_packet(
            packet, MSCP_OP_COMPARE,
            do_transfer(ctlr, packet, MSCP_TRANSFER_COMPARE));
        break;
    case MSCP_OP_ERASE:
        finish_transfer_packet(packet, MSCP_OP_ERASE,
                               do_transfer(ctlr, packet, MSCP_TRANSFER_ERASE));
        break;
    case MSCP_OP_READ:
        finish_transfer_packet(packet, MSCP_OP_READ,
                               do_transfer(ctlr, packet, MSCP_TRANSFER_READ));
        break;
    case MSCP_OP_WRITE:
        finish_transfer_packet(packet, MSCP_OP_WRITE,
                               do_transfer(ctlr, packet, MSCP_TRANSFER_WRITE));
        break;
    default:
        finish_invalid_opcode(packet);
        break;
    }

command_complete:
    if (!write_words(ctlr, desc_packet_addr(ctlr, rsp_desc) - UQ_HDR_OFF,
                     packet, packet[UQ_WORD_LENGTH] + UQ_HDR_OFF)) {
        fatal(ctlr, UQSSP_FATAL_PACKET_WRITE);
        return false;
    }
    if (!release_desc(ctlr, &rsp, rsp_desc))
        return false;
    ctlr->rsp_index = rsp.index;
    (void)post_ring_interrupt(
        ctlr, &rsp, rsp_was_empty && ((rsp_desc & UQ_DESC_FLAG) != 0));
    return true;
}

static bool valid_bus(const vax_mscp_bus *bus)
{
    return bus != NULL && vax_mscp_profile_valid(bus->profile);
}

bool vax_mscp_init(vax_mscp *ctlr, const vax_mscp_bus *bus)
{
    if (ctlr == NULL || !valid_bus(bus))
        return false;

    memset(ctlr, 0, sizeof(*ctlr));
    ctlr->bus = *bus;
    ctlr->profile = bus->profile;
    return true;
}

bool vax_mscp_reset_with_bus(vax_mscp *ctlr, const vax_mscp_bus *bus)
{
    vax_mscp_bus new_bus;
    vax_mscp_unit unit[VAX_MSCP_MAX_UNITS];
    uint16_t last_fail_code;
    uint8_t last_fail_valid;

    if (ctlr == NULL || !valid_bus(bus))
        return false;

    new_bus = *bus;
    last_fail_code = ctlr->last_fail_code;
    last_fail_valid = ctlr->last_fail_valid;
    memcpy(unit, ctlr->unit, sizeof(unit));
    memset(ctlr, 0, sizeof(*ctlr));
    ctlr->bus = new_bus;
    ctlr->profile = new_bus.profile;
    memcpy(ctlr->unit, unit, sizeof(ctlr->unit));
    ctlr->last_fail_code = last_fail_code;
    ctlr->last_fail_valid = last_fail_valid;
    ctlr->sa = UQSSP_STEP1 | UQSSP_DI | UQSSP_MP;
    ctlr->state = VAX_MSCP_STATE_STEP1;
    return true;
}

bool vax_mscp_reset(vax_mscp *ctlr)
{
    if (ctlr == NULL)
        return false;
    return vax_mscp_reset_with_bus(ctlr, &ctlr->bus);
}

uint16_t vax_mscp_read_ip(vax_mscp *ctlr)
{
    if (ctlr->state == VAX_MSCP_STATE_STEP3_PURGE_POLL) {
        enter_step4(ctlr);
        post_init_interrupt(ctlr);
    } else if (ctlr->state == VAX_MSCP_STATE_UP) {
        if (ctlr->last_fail_pending && !post_last_fail_packet(ctlr))
            return 0;
        while (process_command(ctlr)) {
        }
    }
    return 0;
}

uint16_t vax_mscp_read_sa(const vax_mscp *ctlr)
{
    return ctlr->sa;
}

uint16_t vax_mscp_interrupt_vector(const vax_mscp *ctlr)
{
    return ctlr->interrupt_vector;
}

void vax_mscp_write_sa(vax_mscp *ctlr, uint16_t value)
{
    ctlr->saw = value;

    switch (ctlr->state) {
    case VAX_MSCP_STATE_STEP1:
        if ((value & UQSSP_S1_VALID) == 0)
            return;
        if (value & UQSSP_S1_WRAP) {
            ctlr->sa = value;
            ctlr->state = VAX_MSCP_STATE_STEP1_WRAP;
            return;
        }
        ctlr->s1 = value;
        ctlr->interrupt_vector = (value & UQSSP_S1_VECTOR_MASK) << 2;
        ctlr->sa = UQSSP_STEP2 | ((value >> UQSSP_S1_RQ_SHIFT) & 0xffu);
        ctlr->state = VAX_MSCP_STATE_STEP2;
        post_init_interrupt(ctlr);
        return;

    case VAX_MSCP_STATE_STEP1_WRAP:
        ctlr->sa = value;
        return;

    case VAX_MSCP_STATE_STEP2:
        ctlr->comm = value & UQSSP_S2_COMM_LO_MASK;
        ctlr->purge_interrupt = (value & UQSSP_S2_PURGE_INTERRUPT) != 0u;
        ctlr->sa = UQSSP_STEP3 | (ctlr->s1 & UQSSP_S1_STEP3_ECHO_MASK);
        ctlr->state = VAX_MSCP_STATE_STEP3;
        post_init_interrupt(ctlr);
        return;

    case VAX_MSCP_STATE_STEP3: {
        uint32_t raw_comm =
            ctlr->comm | ((uint32_t)(value & UQSSP_S3_COMM_HI_MASK) << 16);
        bool pte_tagged = (raw_comm & ~ctlr->profile->phys_addr_mask) != 0;

        /*
         * NetBSD/vax bus_dma may pass KDB control-area addresses with
         * VAX PTE protection bits still present. The KDB sees physical
         * memory, so ignore those tag bits before using the ring base.
         */
        ctlr->comm = phys_addr(ctlr, raw_comm);
        if ((value & UQSSP_S3_PURGE_POLL) && !pte_tagged) {
            ctlr->sa = 0;
            ctlr->state = VAX_MSCP_STATE_STEP3_PURGE_SA;
            return;
        }
        enter_step4(ctlr);
        post_init_interrupt(ctlr);
        return;
    }

    case VAX_MSCP_STATE_STEP3_PURGE_SA:
        if (value != 0)
            fatal(ctlr, UQSSP_FATAL_PURGE_POLL);
        else
            ctlr->state = VAX_MSCP_STATE_STEP3_PURGE_POLL;
        return;

    case VAX_MSCP_STATE_STEP4:
        if (value & UQSSP_S4_GO) {
            ctlr->sa = 0;
            ctlr->state = VAX_MSCP_STATE_UP;
            ctlr->last_fail_pending =
                ((value & UQSSP_S4_LF) != 0) && ctlr->last_fail_valid;
        }
        return;

    default:
        return;
    }
}

uint32_t vax_mscp_comm_region(const vax_mscp *ctlr)
{
    return ctlr->comm;
}

void vax_mscp_set_unit(vax_mscp *ctlr, uint16_t unit, uint32_t blocks)
{
    bool changed;

    if (unit >= max_units(ctlr))
        return;
    changed = (ctlr->unit[unit].present != (blocks != 0u)) ||
              (ctlr->unit[unit].blocks != blocks);
    ctlr->unit[unit].present = blocks != 0u;
    ctlr->unit[unit].online = 0;
    ctlr->unit[unit].compare_reads = 0;
    ctlr->unit[unit].compare_writes = 0;
    ctlr->unit[unit].write_protected = 0;
    if (changed)
        memset(ctlr->unit[unit].forced_error, 0,
               sizeof(ctlr->unit[unit].forced_error));
    ctlr->unit[unit].blocks = blocks;
}
