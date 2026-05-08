/* uint_bits.h: unsigned integer bit and field helpers */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: X11

#ifndef UINT_BITS_H_
#define UINT_BITS_H_ 0

#include <stdint.h>

#include "ints.h"

// Return a mask for the low width bits of a uint32_t.
//
// width must be in the range 0 through 32. A width of 32 returns all bits set
// without shifting by the type width.
//
// Example: u32_low_mask(5) returns 0b00011111.
//
//     result: 0b000[11111]
static inline uint32_t u32_low_mask(uint_t width)
{
    return (width == 32u) ? UINT32_MAX : ((UINT32_C(1) << width) - 1u);
}

// Return a mask for a uint32_t field.
//
// shift is the number of bits between bit 0 and the low bit of the field.
// width is the width of the field in bits. width must be in the range 0
// through 32, and shift + width must not exceed 32.
//
// Example: u32_field_mask(2, 3) returns 0b00011100.
//
//     result: 0b000[111]00
static inline uint32_t u32_field_mask(uint_t shift, uint_t width)
{
    return u32_low_mask(width) << shift;
}

// Make a positioned uint32_t field from a right-justified source value.
//
// src is first masked to width bits, then shifted left by shift. This
// returns only the positioned field bits; it does not preserve or change bits
// from another value. Use u32_put_field to insert into an existing value.
//
// Example: u32_make_field(0b101101, 3, 4) returns 0b01101000.
//
//     src:    0b0010[1101]
//     result: 0b0[1101]000
static inline uint32_t u32_make_field(uint32_t src, uint_t shift, uint_t width)
{
    return (src & u32_low_mask(width)) << shift;
}

// Get a field from a uint32_t and return it right-justified.
//
// shift is the number of bits between bit 0 and the low bit of the field.
// width is the width of the field in bits. The returned value has the selected
// field in its low bits.
//
// Example: u32_get_field(0b10110110, 2, 3) returns 0b00000101.
//
//     src:    0b101[101]10
//     result: 0b00000[101]
static inline uint32_t u32_get_field(uint32_t src, uint_t shift, uint_t width)
{
    return (src >> shift) & u32_low_mask(width);
}

// Put a right-justified source field value into an existing uint32_t.
//
// dest is the existing value. src is the new field value. The field starts at
// shift and has width bits. src is masked to width bits before insertion. Bits
// outside the field are preserved.
//
// Example: u32_put_field(0b10101010, 0b001101, 2, 3) returns 0b10110110.
//
//     dest:   0b101[010]10
//     src:    0b00000[101]
//     result: 0b101[101]10
static inline uint32_t u32_put_field(uint32_t dest, uint32_t src, uint_t shift,
                                     uint_t width)
{
    uint32_t mask = u32_field_mask(shift, width);

    return (dest & ~mask) | u32_make_field(src, shift, width);
}

// Put an aligned little-endian u8 field into an existing uint32_t dest.
//
// addr is a simulated machine address. Only the low two addr bits are used.
// addr & 3 selects the 8-bit aligned 8-bit field inside the uint32_t. src is
// masked to 8 bits before insertion, and the other bits in dest are preserved.
//
// Example: u32_put_addr_u8_le(0x12345678, 0x1ab, 0x1001) returns 0x1234ab78.
//
//     dest:   0x12 34 [56] 78
//     src:    0x00 00 01 [ab]
//     result: 0x12 34 [ab] 78
static inline uint32_t u32_put_addr_u8_le(uint32_t dest, uint32_t src,
                                          uint32_t addr)
{
    return u32_put_field(dest, src, (uint_t)(addr & 3u) << 3, 8u);
}

// Put an aligned little-endian u16 field into an existing uint32_t dest.
//
// addr is a simulated machine address. Only addr bit 1 is used. addr & 2
// selects the 16-bit aligned 16-bit field inside the uint32_t. src is masked
// to 16 bits before insertion, and the other bits in dest are preserved.
//
// Example: u32_put_addr_u16_le(0x12345678, 0x1abcd, 0x1002) returns
// 0xabcd5678.
//
//     dest:   0x[1234] 5678
//     src:    0x0001 [abcd]
//     result: 0x[abcd] 5678
static inline uint32_t u32_put_addr_u16_le(uint32_t dest, uint32_t src,
                                           uint32_t addr)
{
    return u32_put_field(dest, src, (uint_t)(addr & 2u) << 3, 16u);
}

// Make an aligned little-endian u16 field.
//
// addr is a simulated machine address. Only addr bit 1 is used. addr & 2
// selects the 16-bit aligned 16-bit field inside the uint32_t. src is masked
// to 16 bits before shifting.
//
// Example: u32_make_addr_u16_le(0x1abcd, 0x1002) returns 0xabcd0000.
//
//     src:    0x0001 [abcd]
//     result: 0x[abcd] 0000
static inline uint32_t u32_make_addr_u16_le(uint32_t src, uint32_t addr)
{
    return u32_make_field(src, (uint_t)(addr & 2u) << 3, 16u);
}

// Get an aligned little-endian u8 field from a uint32_t src.
//
// addr is a simulated machine address. Only the low two addr bits are used.
// addr & 3 selects the 8-bit aligned 8-bit field inside the uint32_t. The
// selected field is returned in the low bits.
//
// Example: u32_get_addr_u8_le(0x12345678, 0x1001) returns 0x56.
//
//     src:    0x12 34 [56] 78
//     result: 0x00 00 00 [56]
static inline uint32_t u32_get_addr_u8_le(uint32_t src, uint32_t addr)
{
    return u32_get_field(src, (uint_t)(addr & 3u) << 3, 8u);
}

// Get an aligned little-endian u16 field from a uint32_t src.
//
// addr is a simulated machine address. Only addr bit 1 is used. addr & 2
// selects the 16-bit aligned 16-bit field inside the uint32_t. The selected
// field is returned in the low bits.
//
// Example: u32_get_addr_u16_le(0x12345678, 0x1002) returns 0x1234.
//
//     src:    0x[1234] 5678
//     result: 0x0000 [1234]
static inline uint32_t u32_get_addr_u16_le(uint32_t src, uint32_t addr)
{
    return u32_get_field(src, (uint_t)(addr & 2u) << 3, 16u);
}

// Put a counted little-endian u8 field group into an existing uint32_t dest.
//
// count must be in the range 1 through 4. It says how many adjacent u8 fields
// are affected.
// addr is a simulated machine address. Only the low addr bits are used to
// select the starting field. src is masked to the selected width before
// insertion, and the other bits in dest are preserved. If count is 4, the
// whole uint32_t is replaced. The selected fields must not cross the uint32_t
// boundary; for example, count 3 is valid at addr & 3 values 0 and 1, but not
// 2 or 3.
//
// Example: u32_put_addr_u8_count_le(0x12345678, 0xabcd, 0x1001, 2) returns
// 0x12abcd78.
//
//     dest:   0x12 [34 56] 78
//     src:    0x00 00 [ab cd]
//     result: 0x12 [ab cd] 78
static inline uint32_t u32_put_addr_u8_count_le(uint32_t dest, uint32_t src,
                                                uint32_t addr, uint_t count)
{
    if (count == 4u)
        return src;
    return u32_put_field(dest, src, (uint_t)(addr & 3u) << 3, count << 3);
}

// Make a counted little-endian u8 field group.
//
// count must be in the range 1 through 4. It says how many adjacent u8 fields
// are made. addr is a simulated machine address. Only the low addr bits are
// used to select the starting field. src is masked to the selected width
// before shifting. If count is 4, the whole uint32_t is returned. The selected
// fields must not cross the uint32_t boundary; for example, count 3 is valid
// at addr & 3 values 0 and 1, but not 2 or 3.
//
// Example: u32_make_addr_u8_count_le(0xabcd, 0x1001, 2) returns 0x00abcd00.
//
//     src:    0x00 00 [ab cd]
//     result: 0x00 [ab cd] 00
static inline uint32_t u32_make_addr_u8_count_le(uint32_t src, uint32_t addr,
                                                 uint_t count)
{
    if (count == 4u)
        return src;
    return u32_make_field(src, (uint_t)(addr & 3u) << 3, count << 3);
}

// Get a u8 by u8 index from a right-justified uint32_t src.
//
// index must be in the range 0 through 3. This is useful when splitting a
// larger write value into u8-sized device operations.
//
// Example: u32_get_u8(0x12345678, 1) returns 0x56.
//
//     src:    0x12 34 [56] 78
//     result: 0x00 00 00 [56]
static inline uint32_t u32_get_u8(uint32_t src, uint_t index)
{
    return (src >> (index << 3)) & UINT32_C(0xff);
}

// Get a u16 by u8 index from a right-justified uint32_t src.
//
// u8_index is the u8 offset of the u16 field and must be 0, 1, or 2. This is
// useful when splitting unaligned larger writes into u16-sized device
// operations.
//
// Example: u32_get_u16(0x12345678, 1) returns 0x3456.
//
//     src:    0x12 [34 56] 78
//     result: 0x00 00 [34 56]
static inline uint32_t u32_get_u16(uint32_t src, uint_t u8_index)
{
    return (src >> (u8_index << 3)) & UINT32_C(0xffff);
}

// Combine low and high u16 values into a uint32_t result.
//
// Each input is masked to 16 bits before being placed in the result.
//
// Example: u32_from_u16_pair(0x15678, 0x11234) returns 0x12345678.
//
//     high src: 0x0001 [1234]
//     low src:  0x0001 [5678]
//     result:   0x[1234] [5678]
static inline uint32_t u32_from_u16_pair(uint32_t low, uint32_t high)
{
    return (low & UINT32_C(0xffff)) | ((high & UINT32_C(0xffff)) << 16);
}

// Put a right-justified u16 into the low half of a uint32_t dest.
//
// src is masked to 16 bits before insertion. The high u16 in dest is
// preserved.
//
// Example: u32_put_low_u16(0x12345678, 0x1abcd) returns 0x1234abcd.
//
//     dest:   0x1234 [5678]
//     src:    0x0001 [abcd]
//     result: 0x1234 [abcd]
static inline uint32_t u32_put_low_u16(uint32_t dest, uint32_t src)
{
    return u32_put_field(dest, src, 0u, 16u);
}

// Put a right-justified u16 into the high half of a uint32_t dest.
//
// src is masked to 16 bits before insertion. The low u16 in dest is
// preserved.
//
// Example: u32_put_high_u16(0x12345678, 0x1abcd) returns 0xabcd5678.
//
//     dest:   0x[1234] 5678
//     src:    0x0001 [abcd]
//     result: 0x[abcd] 5678
static inline uint32_t u32_put_high_u16(uint32_t dest, uint32_t src)
{
    return u32_put_field(dest, src, 16u, 16u);
}

// Return the low u16 half of a uint32_t src.
//
// Example: u32_low_u16(0x12345678) returns 0x5678.
//
//     src:    0x1234 [5678]
//     result: 0x0000 [5678]
static inline uint32_t u32_low_u16(uint32_t src)
{
    return src & UINT32_C(0xffff);
}

// Return the high u16 half of a uint32_t src.
//
// Example: u32_high_u16(0x12345678) returns 0x1234.
//
//     src:    0x[1234] 5678
//     result: 0x0000 [1234]
static inline uint32_t u32_high_u16(uint32_t src)
{
    return (src >> 16) & UINT32_C(0xffff);
}

#endif
