#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "sim_defs.h"
#include "sim_sock.h"

static int setup_sock_group(void **state)
{
    (void)state;

    sim_init_sock();
    return 0;
}

static int teardown_sock_group(void **state)
{
    (void)state;

    sim_cleanup_sock();
    return 0;
}

/* Verify bare port input falls back to the default host and null input
   falls back to both defaults. */
static void test_sim_parse_addr_applies_default_host_and_port(void **state)
{
    char host[64];
    char port[64];

    (void)state;

    assert_int_equal(sim_parse_addr("8080", host, sizeof(host), "127.0.0.1",
                                    port, sizeof(port), "23", NULL),
                     0);
    assert_string_equal(host, "127.0.0.1");
    assert_string_equal(port, "8080");

    assert_int_equal(sim_parse_addr(NULL, host, sizeof(host), "localhost", port,
                                    sizeof(port), "1000", NULL),
                     0);
    assert_string_equal(host, "localhost");
    assert_string_equal(port, "1000");
}

/* Verify host:port parsing handles domain-literal IPv6 and rejects
   ports outside the legal range. */
static void
test_sim_parse_addr_handles_ipv6_literals_and_bad_ports(void **state)
{
    char host[64];
    char port[64];

    (void)state;

    assert_int_equal(sim_parse_addr("[::1]:2000", host, sizeof(host), NULL,
                                    port, sizeof(port), NULL, NULL),
                     0);
    assert_string_equal(host, "::1");
    assert_string_equal(port, "2000");

    assert_int_equal(sim_parse_addr("127.0.0.1:70000", host, sizeof(host), NULL,
                                    port, sizeof(port), NULL, NULL),
                     -1);
}

/* Verify validated parsing accepts a matching address and rejects a
   different one. */
static void test_sim_parse_addr_validation_checks_resolved_host(void **state)
{
    char host[64];
    char port[64];

    (void)state;

    assert_int_equal(sim_parse_addr("127.0.0.1:3000", host, sizeof(host), NULL,
                                    port, sizeof(port), NULL, "127.0.0.1"),
                     0);
    assert_int_equal(sim_parse_addr("127.0.0.1:3000", host, sizeof(host), NULL,
                                    port, sizeof(port), NULL, "192.0.2.1"),
                     -1);
}

/* Verify extended parsing separates an optional local port from the
   remote host/port pair. */
static void test_sim_parse_addr_ex_extracts_local_and_remote_ports(void **state)
{
    char host[64];
    char port[64];
    char localport[64];

    (void)state;

    assert_int_equal(sim_parse_addr_ex("2000:example.com:3000", host,
                                       sizeof(host), "localhost", port,
                                       sizeof(port), localport,
                                       sizeof(localport), "1000"),
                     0);
    assert_string_equal(localport, "2000");
    assert_string_equal(host, "example.com");
    assert_string_equal(port, "3000");

    assert_int_equal(sim_parse_addr_ex("4000", host, sizeof(host), "localhost",
                                       port, sizeof(port), localport,
                                       sizeof(localport), "1000"),
                     0);
    assert_string_equal(localport, "");
    assert_string_equal(host, "localhost");
    assert_string_equal(port, "4000");
}

/* Verify ACL checks accept valid standalone CIDR syntax and honor allow
   and deny rules. */
static void test_sim_addr_acl_check_validates_and_matches_rules(void **state)
{
    (void)state;

    assert_int_equal(sim_addr_acl_check("127.0.0.1/24", NULL), 0);
    assert_int_equal(sim_addr_acl_check("127.0.0.1/33", NULL), -1);

    assert_int_equal(sim_addr_acl_check("127.0.0.1", "+127.0.0.0/24"), 0);
    assert_int_equal(sim_addr_acl_check("127.0.0.1", "-127.0.0.0/24"), -1);
    assert_int_equal(
        sim_addr_acl_check("127.0.0.1", "+192.0.2.0/24,+198.51.100.0/24"), -1);
}

static void test_sim_addr_acl_check_rejects_oversized_segments(void **state)
{
    char validate_addr[270];
    char acl[280];

    (void)state;

    memset(validate_addr, '1', 256);
    strlcpy(validate_addr + 256, "/24", sizeof(validate_addr) - 256);
    assert_int_equal(sim_addr_acl_check(validate_addr, NULL), -1);

    acl[0] = '+';
    memset(acl + 1, '1', 260);
    strlcpy(acl + 261, ",+127.0.0.0/24", sizeof(acl) - 261);
    assert_int_equal(sim_addr_acl_check("127.0.0.1", acl), -1);
}

static void
test_sim_sock_convert_ipv4_mapped_ipv6_copies_through_nul(void **state)
{
    char hostname[64];
    static const char mapped[] = "::ffff:198.51.100.42";
    static const char trailer[] = "overread";
    static const char expected_tail[] = "100.42";
    const size_t mapped_len = strlen(mapped);
    const size_t bare_len = strlen("198.51.100.42");

    (void)state;

    memset(hostname, 'Z', sizeof(hostname));
    memcpy(hostname, mapped, mapped_len + 1);
    memcpy(hostname + mapped_len + 1, trailer, sizeof(trailer));

    sim_sock_convert_ipv4_mapped_ipv6(hostname);

    assert_string_equal(hostname, "198.51.100.42");
    assert_memory_equal(hostname + bare_len + 1, expected_tail,
                        sizeof(expected_tail) - 1);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sim_parse_addr_applies_default_host_and_port),
        cmocka_unit_test(
            test_sim_parse_addr_handles_ipv6_literals_and_bad_ports),
        cmocka_unit_test(test_sim_parse_addr_validation_checks_resolved_host),
        cmocka_unit_test(
            test_sim_parse_addr_ex_extracts_local_and_remote_ports),
        cmocka_unit_test(test_sim_addr_acl_check_validates_and_matches_rules),
        cmocka_unit_test(
            test_sim_addr_acl_check_rejects_oversized_segments),
        cmocka_unit_test(
            test_sim_sock_convert_ipv4_mapped_ipv6_copies_through_nul),
    };

    return cmocka_run_group_tests(tests, setup_sock_group, teardown_sock_group);
}
