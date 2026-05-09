#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "h316_hi_internal.h"

/*
 * Reset the host interface data block used by the 1822 leader conversion
 * tests.  The production code stores the packet being converted in rxdata.
 */
static void reset_hidb(HIDB *hidb)
{
    memset(hidb, 0, sizeof(*hidb));
}

/*
 * Write a host-to-IMP 1822L leader into rxdata, leaving rxdata[0] for the
 * simulator-private packet flags word used by the H316 UDP shim.
 */
static void write_long_leader(HIDB *hidb, uint16_t nflags, uint16_t mtype,
                              uint16_t htype, uint16_t host, uint16_t imp, uint16_t id,
                              uint16_t stype, uint16_t reserved)
{
    hidb->rxdata[1] = NEW_FORMAT_FLAG;
    hidb->rxdata[2] = (nflags << 8) | mtype;
    hidb->rxdata[3] = (htype << 8) | host;
    hidb->rxdata[4] = imp;
    hidb->rxdata[5] = (id << 4) | stype;
    hidb->rxdata[6] = reserved;
}

/*
 * Packets without a full 1822L long leader are not converted.  This documents
 * the boundary that lets mixed short/long traffic pass through unchanged.
 */
static void test_non_long_packet_is_left_unchanged(void **state)
{
    HIDB hidb;

    (void)state;
    reset_hidb(&hidb);
    hidb.rxdata[1] = 012345;
    hidb.rxdata[2] = 067123;

    assert_int_equal(hi_convert_long_to_short(&hidb, 3), 3);
    assert_int_equal(hidb.rxdata[1], 012345);
    assert_int_equal(hidb.rxdata[2], 067123);
}

/*
 * A regular host-to-IMP long leader is shortened to the old two-word leader.
 * Word 6 is reserved in this direction, so a nonzero value must not affect the
 * result; only the actual payload words should be moved behind the new leader.
 */
static void test_regular_long_leader_converts_and_moves_payload(void **state)
{
    HIDB hidb;
    const uint16_t id = 0123;
    uint16_t expected_flags;

    (void)state;
    reset_hidb(&hidb);
    write_long_leader(&hidb, NLEADER_TRACE | NLEADER_OCTAL, LEADER_REGULAR,
                      NLEADER_PRIORITY, 2, 5, id, 0, 0177777);
    hidb.rxdata[7] = 0111111;
    hidb.rxdata[8] = 0122222;
    hidb.rxdata[9] = 0133333;
    hidb.rxdata[10] = 0144444;

    assert_int_equal(hi_convert_long_to_short(&hidb, 9), 5);

    expected_flags = OLEADER_TRACE | OLEADER_OCTAL | OLEADER_PRIORITY;
    assert_int_equal(hidb.rxdata[1], (expected_flags << 12) |
                                         (LEADER_REGULAR << 8) | (2 << 6) | 5);
    assert_int_equal(hidb.rxdata[2], id << 4);
    assert_int_equal(hidb.rxdata[3], 0111111);
    assert_int_equal(hidb.rxdata[4], 0122222);
    assert_int_equal(hidb.rxdata[5], (id << 4));
}

/*
 * A long-leader NOP records the padding count used by following regular
 * messages.  The NOP itself has no payload after conversion.
 */
static void test_nop_long_leader_records_padding(void **state)
{
    HIDB hidb;

    (void)state;
    reset_hidb(&hidb);
    write_long_leader(&hidb, 0, LEADER_NOP, 0, 1, 2, 077, 2, 0);

    assert_int_equal(hi_convert_long_to_short(&hidb, 7), 3);
    assert_int_equal(hidb.padding, 2);
    assert_int_equal(hidb.rxdata[1], (LEADER_NOP << 8) | (1 << 6) | 2);
    assert_int_equal(hidb.rxdata[2], 077 << 4);
}

/*
 * Regular long leaders following a NOP skip the remembered leader padding
 * before moving the payload to its short-leader position.
 */
static void test_regular_long_leader_skips_recorded_padding(void **state)
{
    HIDB hidb;
    const uint16_t id = 0456;

    (void)state;
    reset_hidb(&hidb);
    hidb.padding = 2;
    write_long_leader(&hidb, 0, LEADER_REGULAR, 0, 1, 2, id, 0, 0);
    hidb.rxdata[7] = 0121212;
    hidb.rxdata[8] = 0131313;
    hidb.rxdata[9] = 0141414;
    hidb.rxdata[10] = 0151515;
    hidb.rxdata[11] = 0161616;

    assert_int_equal(hi_convert_long_to_short(&hidb, 11), 5);
    assert_int_equal(hidb.rxdata[3], 0141414);
    assert_int_equal(hidb.rxdata[4], 0151515);
    assert_int_equal(hidb.rxdata[5], (id << 4));
}

/*
 * If the UDP packet is too short to contain the remembered long-leader
 * padding, conversion rejects it rather than copying from outside the packet.
 */
static void test_regular_long_leader_rejects_truncated_padding(void **state)
{
    HIDB hidb;

    (void)state;
    reset_hidb(&hidb);
    hidb.padding = 3;
    write_long_leader(&hidb, 0, LEADER_REGULAR, 0, 1, 2, 0, 0, 0);

    assert_int_equal(hi_convert_long_to_short(&hidb, 9), 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_non_long_packet_is_left_unchanged),
        cmocka_unit_test(test_regular_long_leader_converts_and_moves_payload),
        cmocka_unit_test(test_nop_long_leader_records_padding),
        cmocka_unit_test(test_regular_long_leader_skips_recorded_padding),
        cmocka_unit_test(test_regular_long_leader_rejects_truncated_padding),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
