/* sel32_arith_internal.h: internal SEL-32 arithmetic helpers */

#ifndef SEL32_ARITH_INTERNAL_H_
#define SEL32_ARITH_INTERNAL_H_ 1

#include <stdbool.h>
#include <stdint.h>

#include "sel32_defs.h"

/* Interpret a 32-bit machine word as a signed two's-complement value. */
static inline int32_t sel32_word_as_signed(uint32_t word)
{
    if ((word & MSIGN) == 0)
        return (int32_t)word;
    return (int32_t)((int64_t)word - ((int64_t)1 << 32));
}

/* Compare two signed 32-bit CAMx operands without subtracting them. */
static inline int sel32_compare_signed_words(uint32_t left, uint32_t right)
{
    int32_t left_value = sel32_word_as_signed(left);
    int32_t right_value = sel32_word_as_signed(right);

    return (left_value > right_value) - (left_value < right_value);
}

/* Compare two signed 64-bit CAMx operands without subtracting them. */
static inline int sel32_compare_signed_doubles(uint64_t left, uint64_t right)
{
    bool left_negative = (left & DMSIGN) != 0;
    bool right_negative = (right & DMSIGN) != 0;

    if (left_negative != right_negative)
        return left_negative ? -1 : 1;
    if (left == right)
        return 0;
    return left < right ? -1 : 1;
}

/* Return the CAMx register result for a signed comparison order. */
static inline uint64_t sel32_cam_result_from_order(int order)
{
    if (order > 0)
        return 1;
    if (order == 0)
        return 0;
    return (uint64_t)-1;
}

/* Return the CAMx result for signed 32-bit machine-word operands. */
static inline uint64_t sel32_cam_word_result(uint32_t left, uint32_t right)
{
    return sel32_cam_result_from_order(sel32_compare_signed_words(left, right));
}

/* Return the CAMx result for signed 64-bit machine-doubleword operands. */
static inline uint64_t sel32_cam_double_result(uint64_t left, uint64_t right)
{
    return sel32_cam_result_from_order(
        sel32_compare_signed_doubles(left, right));
}

#endif
