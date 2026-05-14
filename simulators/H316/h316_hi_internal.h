/* h316_hi_internal.h: internal 1822 host interface conversion helpers */

#ifndef H316_HI_INTERNAL_H_
#define H316_HI_INTERNAL_H_ 1

#include <stdint.h>
#include <string.h>

#include "h316_imp.h"

/* Old message type field set to this to signal new format leader. */
#define NEW_FORMAT_FLAG (15 << 8)

/* 1822 message types (or subtypes). */
#define LEADER_REGULAR 000
#define LEADER_UNCONTROLLED 003
#define LEADER_NOP 004

/* 1822 new leader flags. */
#define NLEADER_TRACE 010
#define NLEADER_OCTAL 004
#define NLEADER_FOR_IMP 252
#define NLEADER_PRIORITY 0200

/* 1822 old leader flags. */
#define OLEADER_PRIORITY 010
#define OLEADER_FOR_IMP 004
#define OLEADER_TRACE 002
#define OLEADER_OCTAL 001

/*
 * Convert one host-to-IMP 1822L long leader in the host-interface receive
 * buffer to the short leader format expected by the 1974 IMP software.
 *
 * The incoming buffer has a simulator-private flags word at rxdata[0],
 * followed by the six-word 1822L leader.  Host-to-IMP 1822L word 6 is
 * reserved, so it is intentionally ignored.  The function returns the new
 * word count, the original count for packets that are not long-leader
 * packets, or zero for long leaders that cannot be represented.
 */
static inline int16_t hi_convert_long_to_short(HIDB *hidb, int16_t count)
{
    uint16_t nflags, mtype, htype, host, imp, id, stype;
    uint16_t oflags;
    uint16_t *data = hidb->rxdata;

    if (count == 0)
        return 0;

    if (count < 7 || (data[1] & 0x0F00) != NEW_FORMAT_FLAG)
        return count; /* This is not a long leader message. */

    nflags = (data[2] & 0x0F00) >> 8;
    mtype = data[2] & 0xFF;
    htype = (data[3] & 0xFF00) >> 8;
    host = data[3] & 0xFF;
    imp = data[4];
    id = (data[5] & 0xFFF0) >> 4;
    stype = data[5] & 0x000F;

    /* Keep track of padding. */
    if (mtype == LEADER_NOP)
        hidb->padding = stype;

    /* Sorry, can't handle these addresses. */
    if (host > 3)
        return 0;
    if (imp > 63)
        return 0;

    if (mtype == LEADER_REGULAR && stype == LEADER_UNCONTROLLED)
        mtype = LEADER_UNCONTROLLED, stype = 0;
    else if (mtype == LEADER_NOP)
        stype = 0;

    oflags = 0;
    if (nflags & NLEADER_TRACE)
        oflags |= OLEADER_TRACE;
    if (nflags & NLEADER_OCTAL)
        oflags |= OLEADER_OCTAL;
    if (host >= NLEADER_FOR_IMP) {
        oflags |= OLEADER_FOR_IMP;
        host -= NLEADER_FOR_IMP;
    }
    if (htype & NLEADER_PRIORITY)
        oflags |= OLEADER_PRIORITY;

    data[1] = (oflags << 12) | (mtype << 8) | (host << 6) | imp;
    data[2] = (id << 4) | stype;

    if (mtype == LEADER_REGULAR) {
        int payload_count = count - 7 - hidb->padding;

        if (payload_count < 0)
            return 0;
        memmove(&data[3], &data[7 + hidb->padding],
                sizeof(data[0]) * (size_t)payload_count);
        count = (int16_t)(3 + payload_count);
    } else {
        count = 3;
    }

    return count;
}

#endif /* H316_HI_INTERNAL_H_ */
