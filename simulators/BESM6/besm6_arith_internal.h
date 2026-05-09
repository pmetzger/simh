/*
 * Private BESM-6 arithmetic helpers used by host-side unit tests.
 */
#ifndef BESM6_ARITH_INTERNAL_H_
#define BESM6_ARITH_INTERNAL_H_

#include <stdint.h>

#include "besm6_defs.h"

/*
 * Return the sign-fill bits for a negative BESM-6 ALU mantissa after a
 * logical right shift. The low_zero_bits least-significant bits remain clear;
 * the remaining bits up through the 42-bit ALU mantissa are set.
 */
static inline uint64_t besm6_alu_sign_extension_mask(unsigned low_zero_bits)
{
    return BITS42 & ~(((uint64_t)1 << low_zero_bits) - 1);
}

#endif
