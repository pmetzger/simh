/* sel32_arith_internal.h: internal SEL-32 arithmetic helpers */

#ifndef SEL32_ARITH_INTERNAL_H_
#define SEL32_ARITH_INTERNAL_H_ 0

#include <stdbool.h>
#include "sel32_defs.h"

/* Interpret a 32-bit machine word as a signed two's-complement value. */
static inline int32 sel32_word_as_signed(uint32 word)
{
    if ((word & MSIGN) == 0)
        return (int32)word;
    return (int32)((t_int64)word - ((t_int64)1 << 32));
}

/* Compare two signed 32-bit CAMx operands without subtracting them. */
static inline int sel32_compare_signed_words(uint32 left, uint32 right)
{
    int32 left_value = sel32_word_as_signed(left);
    int32 right_value = sel32_word_as_signed(right);

    return (left_value > right_value) - (left_value < right_value);
}

/* Compare two signed 64-bit CAMx operands without subtracting them. */
static inline int sel32_compare_signed_doubles(t_uint64 left, t_uint64 right)
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
static inline t_uint64 sel32_cam_result_from_order(int order)
{
    if (order > 0)
        return 1;
    if (order == 0)
        return 0;
    return (t_uint64)-1;
}

/* Return the CAMx result for signed 32-bit machine-word operands. */
static inline t_uint64 sel32_cam_word_result(uint32 left, uint32 right)
{
    return sel32_cam_result_from_order(sel32_compare_signed_words(left, right));
}

/* Return the CAMx result for signed 64-bit machine-doubleword operands. */
static inline t_uint64 sel32_cam_double_result(t_uint64 left, t_uint64 right)
{
    return sel32_cam_result_from_order(
        sel32_compare_signed_doubles(left, right));
}

#endif
