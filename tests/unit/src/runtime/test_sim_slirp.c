#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "sim_slirp.h"
#include "sim_types.h"

enum {
    max_fake_calls = 8,
    ether_addr_len = 6,
    ether_type_arp = 0x0806,
    ether_type_ipv4 = 0x0800,
    arp_request = 1,
    arp_reply = 2,
    ipv4_proto_udp = 17,
    dhcp_client_port = 68,
    dhcp_server_port = 67,
    dhcp_bootrequest = 1,
    dhcp_bootreply = 2,
    dhcp_discover = 1,
    dhcp_offer = 2,
    dhcp_option_message_type = 53,
    dhcp_option_router = 3,
    dhcp_option_dns = 6,
    dhcp_option_server_id = 54,
    dhcp_option_end = 255,
    dhcp_fixed_len = 236,
    dhcp_options_offset = 42 + dhcp_fixed_len,
    ipv4_proto_icmp = 1,
    icmp_echo_reply = 0,
    icmp_echo_request = 8,
};

struct fake_forward_call {
    int protocol;
    int host_port;
    struct in_addr guest_addr;
    int guest_port;
};

struct fake_backend_state {
    int open_count;
    int close_count;
    int add_count;
    int remove_count;
    int input_count;
    int fill_count;
    int poll_count;
    void *opaque;
    sim_slirp_config open_config;
    struct fake_forward_call add_calls[max_fake_calls];
    struct fake_forward_call remove_calls[max_fake_calls];
    uint8_t last_input[1518];
    int last_input_size;
};

struct callback_capture {
    int packet_count;
    uint8_t last_packet[1518];
    int last_packet_size;
};

static struct fake_backend_state fake_backend_state;
static const sim_slirp_backend *saved_backend;

static void assert_ipv4_equal(struct in_addr actual, const char *expected)
{
    struct in_addr parsed;

    assert_int_equal(inet_pton(AF_INET, expected, &parsed), 1);
    assert_int_equal((uint32_t)actual.s_addr, (uint32_t)parsed.s_addr);
}

static void assert_ipv6_equal(struct in6_addr actual, const char *expected)
{
    struct in6_addr parsed;

    assert_int_equal(inet_pton(AF_INET6, expected, &parsed), 1);
    assert_memory_equal(&actual, &parsed, sizeof(actual));
}

static void parse_config(sim_slirp_config *config, const char *args)
{
    char errbuf[256];

    errbuf[0] = '\0';
    sim_slirp_config_init(config);
    assert_int_equal(
        sim_slirp_config_parse(config, args, errbuf, sizeof(errbuf)), 0);
    assert_string_equal(errbuf, "");
}

static void reset_fake_backend(void)
{
    memset(&fake_backend_state, 0, sizeof(fake_backend_state));
}

static void *fake_open(const sim_slirp_config *config, void *opaque)
{
    ++fake_backend_state.open_count;
    fake_backend_state.opaque = opaque;
    fake_backend_state.open_config = *config;
    return &fake_backend_state;
}

static void fake_close(void *state)
{
    assert_ptr_equal(state, &fake_backend_state);
    ++fake_backend_state.close_count;
}

static int fake_add_hostfwd(void *state, int is_udp, struct in_addr host_addr,
                            int host_port, struct in_addr guest_addr,
                            int guest_port)
{
    struct fake_forward_call *call;

    assert_ptr_equal(state, &fake_backend_state);
    assert_int_equal(host_addr.s_addr, htonl(INADDR_ANY));
    assert_true(fake_backend_state.add_count < max_fake_calls);
    call = &fake_backend_state.add_calls[fake_backend_state.add_count++];
    call->protocol = is_udp;
    call->host_port = host_port;
    call->guest_addr = guest_addr;
    call->guest_port = guest_port;
    return 0;
}

static int fake_remove_hostfwd(void *state, int is_udp,
                               struct in_addr host_addr, int host_port)
{
    struct fake_forward_call *call;

    assert_ptr_equal(state, &fake_backend_state);
    assert_int_equal(host_addr.s_addr, htonl(INADDR_ANY));
    assert_true(fake_backend_state.remove_count < max_fake_calls);
    call = &fake_backend_state.remove_calls[fake_backend_state.remove_count++];
    call->protocol = is_udp;
    call->host_port = host_port;
    return 0;
}

static void fake_input(void *state, const uint8_t *packet, int packet_size)
{
    assert_ptr_equal(state, &fake_backend_state);
    assert_true(packet_size <= (int)sizeof(fake_backend_state.last_input));
    ++fake_backend_state.input_count;
    fake_backend_state.last_input_size = packet_size;
    memcpy(fake_backend_state.last_input, packet, (size_t)packet_size);
}

static void fake_pollfds_fill(void *state, void *pollfds, uint32_t *timeout)
{
    (void)pollfds;
    assert_ptr_equal(state, &fake_backend_state);
    assert_non_null(timeout);
    ++fake_backend_state.fill_count;
}

static void fake_pollfds_poll(void *state, void *pollfds, int select_error)
{
    (void)pollfds;
    assert_ptr_equal(state, &fake_backend_state);
    assert_int_equal(select_error, 0);
    ++fake_backend_state.poll_count;
}

static void fake_connection_info(void *state, FILE *st)
{
    assert_ptr_equal(state, &fake_backend_state);
    assert_non_null(st);
}

static const sim_slirp_backend fake_backend = {
    fake_open,           fake_close,           fake_add_hostfwd,
    fake_remove_hostfwd, fake_input,           fake_pollfds_fill,
    fake_pollfds_poll,   fake_connection_info, 0};

static void capture_packet(void *opaque, const uchar_t *buf, int len)
{
    struct callback_capture *capture = (struct callback_capture *)opaque;

    assert_true(len <= (int)sizeof(capture->last_packet));
    ++capture->packet_count;
    capture->last_packet_size = len;
    memcpy(capture->last_packet, buf, (size_t)len);
}

static void put_be16(uint8_t *ptr, uint16_t value)
{
    ptr[0] = (uint8_t)(value >> 8);
    ptr[1] = (uint8_t)value;
}

static void put_be32(uint8_t *ptr, uint32_t value)
{
    ptr[0] = (uint8_t)(value >> 24);
    ptr[1] = (uint8_t)(value >> 16);
    ptr[2] = (uint8_t)(value >> 8);
    ptr[3] = (uint8_t)value;
}

static uint16_t get_be16(const uint8_t *ptr)
{
    return (uint16_t)((ptr[0] << 8) | ptr[1]);
}

static void put_ipv4(uint8_t *ptr, const char *addr)
{
    struct in_addr parsed;

    assert_int_equal(inet_pton(AF_INET, addr, &parsed), 1);
    memcpy(ptr, &parsed.s_addr, sizeof(parsed.s_addr));
}

static void assert_ipv4_bytes_equal(const uint8_t *actual, const char *expected)
{
    struct in_addr parsed;

    assert_int_equal(inet_pton(AF_INET, expected, &parsed), 1);
    assert_memory_equal(actual, &parsed.s_addr, sizeof(parsed.s_addr));
}

static void assert_ether_dest_is_guest_or_broadcast(const uint8_t *packet,
                                                    const uint8_t *guest_mac)
{
    static const uint8_t broadcast[ether_addr_len] = {0xff, 0xff, 0xff,
                                                    0xff, 0xff, 0xff};

    assert_true((memcmp(packet, guest_mac, ether_addr_len) == 0) ||
                (memcmp(packet, broadcast, ether_addr_len) == 0));
}

static uint16_t ipv4_header_checksum(const uint8_t *header, size_t len)
{
    uint32_t sum = 0;
    size_t i;

    assert_int_equal(len % 2, 0);
    for (i = 0; i < len; i += 2)
        sum += get_be16(header + i);
    while (sum > 0xffff)
        sum = (sum & 0xffff) + (sum >> 16);
    return (uint16_t)~sum;
}

static uint16_t internet_checksum(const uint8_t *buf, size_t len)
{
    uint32_t sum = 0;

    while (len > 1) {
        sum += get_be16(buf);
        buf += 2;
        len -= 2;
    }
    if (len != 0)
        sum += (uint16_t)(*buf << 8);
    while (sum > 0xffff)
        sum = (sum & 0xffff) + (sum >> 16);
    return (uint16_t)~sum;
}

static void build_arp_request(uint8_t *packet, const uint8_t *guest_mac,
                              const char *guest_addr, const char *target_addr)
{
    static const uint8_t broadcast[ether_addr_len] = {0xff, 0xff, 0xff,
                                                    0xff, 0xff, 0xff};

    memset(packet, 0, 42);
    memcpy(packet, broadcast, ether_addr_len);
    memcpy(packet + 6, guest_mac, ether_addr_len);
    put_be16(packet + 12, ether_type_arp);
    put_be16(packet + 14, 1);
    put_be16(packet + 16, 0x0800);
    packet[18] = ether_addr_len;
    packet[19] = 4;
    put_be16(packet + 20, arp_request);
    memcpy(packet + 22, guest_mac, ether_addr_len);
    put_ipv4(packet + 28, guest_addr);
    put_ipv4(packet + 38, target_addr);
}

static size_t build_icmp_echo_request(uint8_t *packet, const uint8_t *guest_mac,
                                      const uint8_t *host_mac)
{
    static const uint8_t payload[] = {'z', 'i', 'm', 'h'};
    uint8_t *ip = packet + 14;
    uint8_t *icmp = packet + 34;
    size_t icmp_len = 8 + sizeof(payload);
    size_t ip_len = 20 + icmp_len;

    memset(packet, 0, 14 + ip_len);
    memcpy(packet, host_mac, ether_addr_len);
    memcpy(packet + 6, guest_mac, ether_addr_len);
    put_be16(packet + 12, ether_type_ipv4);

    ip[0] = 0x45;
    put_be16(ip + 2, (uint16_t)ip_len);
    ip[8] = 64;
    ip[9] = ipv4_proto_icmp;
    put_ipv4(ip + 12, "10.0.2.15");
    put_ipv4(ip + 16, "10.0.2.2");
    put_be16(ip + 10, ipv4_header_checksum(ip, 20));

    icmp[0] = icmp_echo_request;
    put_be16(icmp + 4, 0x1234);
    put_be16(icmp + 6, 1);
    memcpy(icmp + 8, payload, sizeof(payload));
    put_be16(icmp + 2, internet_checksum(icmp, icmp_len));
    return 14 + ip_len;
}

static size_t build_dhcp_discover(uint8_t *packet, const uint8_t *guest_mac,
                                  uint32_t xid)
{
    static const uint8_t broadcast[ether_addr_len] = {0xff, 0xff, 0xff,
                                                    0xff, 0xff, 0xff};
    uint8_t *ip = packet + 14;
    uint8_t *udp = packet + 34;
    uint8_t *dhcp = packet + 42;
    uint8_t *option = dhcp + dhcp_fixed_len;
    size_t dhcp_len;
    size_t udp_len;
    size_t ip_len;

    memset(packet, 0, 300);
    memcpy(packet, broadcast, ether_addr_len);
    memcpy(packet + 6, guest_mac, ether_addr_len);
    put_be16(packet + 12, ether_type_ipv4);

    dhcp[0] = dhcp_bootrequest;
    dhcp[1] = 1;
    dhcp[2] = ether_addr_len;
    put_be32(dhcp + 4, xid);
    put_be16(dhcp + 10, 0x8000);
    memcpy(dhcp + 28, guest_mac, ether_addr_len);

    option[0] = 99;
    option[1] = 130;
    option[2] = 83;
    option[3] = 99;
    option += 4;
    option[0] = dhcp_option_message_type;
    option[1] = 1;
    option[2] = dhcp_discover;
    option += 3;
    option[0] = 55;
    option[1] = 3;
    option[2] = 1;
    option[3] = dhcp_option_router;
    option[4] = dhcp_option_dns;
    option += 5;
    *option++ = dhcp_option_end;

    dhcp_len = (size_t)(option - dhcp);
    udp_len = 8 + dhcp_len;
    ip_len = 20 + udp_len;

    ip[0] = 0x45;
    put_be16(ip + 2, (uint16_t)ip_len);
    ip[8] = 64;
    ip[9] = ipv4_proto_udp;
    put_ipv4(ip + 12, "0.0.0.0");
    put_ipv4(ip + 16, "255.255.255.255");
    put_be16(ip + 10, ipv4_header_checksum(ip, 20));

    put_be16(udp, dhcp_client_port);
    put_be16(udp + 2, dhcp_server_port);
    put_be16(udp + 4, (uint16_t)udp_len);
    return 14 + ip_len;
}

static const uint8_t *find_dhcp_option(const uint8_t *packet, size_t packet_size,
                                     int option_code, size_t *option_len)
{
    const uint8_t *ptr = packet + dhcp_options_offset;
    const uint8_t *end = packet + packet_size;

    if (packet_size < dhcp_options_offset + 4)
        return NULL;
    assert_memory_equal(ptr, "\x63\x82\x53\x63", 4);
    ptr += 4;
    while (ptr < end) {
        int code = *ptr++;

        if (code == dhcp_option_end)
            return NULL;
        if (code == 0)
            continue;
        if (ptr >= end)
            return NULL;
        if ((size_t)(end - ptr) < (size_t)*ptr + 1)
            return NULL;
        if (code == option_code) {
            *option_len = *ptr;
            return ptr + 1;
        }
        ptr += 1 + *ptr;
    }
    return NULL;
}

static int setup_slirp_tests(void **state)
{
    (void)state;

    sim_init_sock();
    saved_backend = sim_slirp_set_backend_for_test(&fake_backend);
    return 0;
}

static int teardown_slirp_tests(void **state)
{
    (void)state;

    sim_slirp_set_backend_for_test(saved_backend);
    sim_cleanup_sock();
    return 0;
}

/* Verify that an empty nat: option string produces the historical defaults. */
static void test_slirp_parse_default_config(void **state)
{
    sim_slirp_config config;

    (void)state;

    parse_config(&config, "");
    assert_ipv4_equal(config.gateway, "10.0.2.2");
    assert_ipv4_equal(config.netmask, "255.255.255.0");
    assert_ipv4_equal(config.network, "10.0.2.0");
    assert_int_equal(config.maskbits, 24);
    assert_int_equal(config.ipv6_enabled, 1);
    assert_ipv6_equal(config.ipv6_prefix, "fd00::");
    assert_int_equal(config.ipv6_prefix_len, 64);
    assert_ipv6_equal(config.ipv6_gateway, "fd00::2");
    assert_ipv6_equal(config.ipv6_nameserver, "fd00::3");
    assert_int_equal(config.dhcp_enabled, 1);
    assert_ipv4_equal(config.dhcp_start, "10.0.2.15");
    assert_ipv4_equal(config.nameserver, "10.0.2.3");
    assert_null(config.tftp_path);
    assert_null(config.boot_file);
    assert_null(config.dns_search_domains);
    assert_null(config.forward_rules);
    sim_slirp_config_free(&config);
}

/* Verify explicit gateway and network options drive derived defaults. */
static void test_slirp_parse_gateway_and_network_config(void **state)
{
    sim_slirp_config config;

    (void)state;

    parse_config(&config, "GATEWAY=192.0.2.1/25");
    assert_ipv4_equal(config.gateway, "192.0.2.1");
    assert_ipv4_equal(config.netmask, "255.255.255.128");
    assert_ipv4_equal(config.network, "192.0.2.0");
    assert_ipv4_equal(config.dhcp_start, "192.0.2.15");
    assert_ipv4_equal(config.nameserver, "192.0.2.3");
    sim_slirp_config_free(&config);

    parse_config(&config, "NETWORK=198.51.100.0/24,GATEWAY=198.51.100.1");
    assert_ipv4_equal(config.gateway, "198.51.100.1");
    assert_ipv4_equal(config.netmask, "255.255.255.0");
    assert_ipv4_equal(config.network, "198.51.100.0");
    assert_ipv4_equal(config.dhcp_start, "198.51.100.15");
    assert_ipv4_equal(config.nameserver, "198.51.100.3");
    sim_slirp_config_free(&config);
}

/* Verify a host-bit-zero gateway is adjusted to a usable host address. */
static void test_slirp_parse_adjusts_network_address_gateway(void **state)
{
    sim_slirp_config config;

    (void)state;

    parse_config(&config, "GATEWAY=203.0.113.0/24");
    assert_ipv4_equal(config.gateway, "203.0.113.2");
    assert_ipv4_equal(config.network, "203.0.113.0");
    sim_slirp_config_free(&config);
}

/* Verify IPv6 NAT options are parsed and derive useful defaults. */
static void test_slirp_parse_ipv6_options(void **state)
{
    sim_slirp_config config;

    (void)state;

    parse_config(&config, "IPV6PREFIX=fd42:1234:5678::99/64");
    assert_int_equal(config.ipv6_enabled, 1);
    assert_ipv6_equal(config.ipv6_prefix, "fd42:1234:5678::");
    assert_int_equal(config.ipv6_prefix_len, 64);
    assert_ipv6_equal(config.ipv6_gateway, "fd42:1234:5678::2");
    assert_ipv6_equal(config.ipv6_nameserver, "fd42:1234:5678::3");
    sim_slirp_config_free(&config);

    parse_config(&config, "IPV6PREFIX=fd42:1234::/64,"
                          "IPV6GATEWAY=fd42:1234::99,"
                          "IPV6DNS=fd42:1234::53");
    assert_int_equal(config.ipv6_enabled, 1);
    assert_ipv6_equal(config.ipv6_gateway, "fd42:1234::99");
    assert_ipv6_equal(config.ipv6_nameserver, "fd42:1234::53");
    sim_slirp_config_free(&config);

    parse_config(&config, "NOIPV6");
    assert_int_equal(config.ipv6_enabled, 0);
    sim_slirp_config_free(&config);
}

/* Verify DHCP start-address and NODHCP options are parsed. */
static void test_slirp_parse_dhcp_options(void **state)
{
    sim_slirp_config config;

    (void)state;

    parse_config(&config, "DHCP=10.0.2.42");
    assert_int_equal(config.dhcp_enabled, 1);
    assert_ipv4_equal(config.dhcp_start, "10.0.2.42");
    sim_slirp_config_free(&config);

    parse_config(&config, "NODHCP");
    assert_int_equal(config.dhcp_enabled, 0);
    assert_int_equal(config.dhcp_start.s_addr, 0);
    sim_slirp_config_free(&config);
}

/* Verify DNS, DNS search, TFTP path, and boot-file options are parsed. */
static void test_slirp_parse_dns_tftp_and_boot_options(void **state)
{
    sim_slirp_config config;

    (void)state;

    parse_config(&config, "DNS=10.0.2.53,DNSSEARCH=one.test:two.test,"
                          "TFTP=/tmp/tftp,BOOTFILE=boot.img");
    assert_ipv4_equal(config.nameserver, "10.0.2.53");
    assert_non_null(config.dns_search_domains);
    assert_string_equal(config.dns_search_domains[0], "one.test");
    assert_string_equal(config.dns_search_domains[1], "two.test");
    assert_null(config.dns_search_domains[2]);
    assert_string_equal(config.tftp_path, "/tmp/tftp");
    assert_string_equal(config.boot_file, "boot.img");
    sim_slirp_config_free(&config);

    parse_config(&config, "NAMESERVER=10.0.2.54");
    assert_ipv4_equal(config.nameserver, "10.0.2.54");
    sim_slirp_config_free(&config);
}

/* Verify TCP and UDP host-forwarding rules are parsed and ordered. */
static void test_slirp_parse_forwarding_rules(void **state)
{
    sim_slirp_config config;
    sim_slirp_forward_rule *rule;

    (void)state;

    parse_config(&config, "TCP=8023:10.0.2.15:23,"
                          "UDP=8053:10.0.2.15:53");
    rule = config.forward_rules;
    assert_non_null(rule);
    assert_int_equal(rule->protocol, sim_slirp_forward_udp);
    assert_int_equal(rule->host_port, 8053);
    assert_ipv4_equal(rule->guest_addr, "10.0.2.15");
    assert_int_equal(rule->guest_port, 53);

    rule = rule->next;
    assert_non_null(rule);
    assert_int_equal(rule->protocol, sim_slirp_forward_tcp);
    assert_int_equal(rule->host_port, 8023);
    assert_ipv4_equal(rule->guest_addr, "10.0.2.15");
    assert_int_equal(rule->guest_port, 23);
    assert_null(rule->next);
    sim_slirp_config_free(&config);
}

static void assert_parse_fails(const char *args, const char *message)
{
    sim_slirp_config config;
    char errbuf[256];

    errbuf[0] = '\0';
    sim_slirp_config_init(&config);
    assert_int_equal(
        sim_slirp_config_parse(&config, args, errbuf, sizeof(errbuf)), -1);
    assert_string_equal(errbuf, message);
    sim_slirp_config_free(&config);
}

/* Verify missing option values fail with specific diagnostics. */
static void test_slirp_parse_reports_missing_values(void **state)
{
    (void)state;

    assert_parse_fails("TFTP=", "Missing TFTP Path");
    assert_parse_fails("BOOTFILE=", "Missing DHCP Boot file name");
    assert_parse_fails("DNS=", "Missing nameserver");
    assert_parse_fails("DNSSEARCH=", "Missing DNS search list");
    assert_parse_fails("GATEWAY=", "Missing host");
    assert_parse_fails("NETWORK=", "Missing network");
    assert_parse_fails("TCP=", "Missing TCP port mapping");
    assert_parse_fails("UDP=", "Missing UDP port mapping");
}

/* Verify malformed NAT options fail with specific diagnostics. */
static void test_slirp_parse_reports_invalid_values(void **state)
{
    char long_forward_rule[5 * CBUFSIZE];
    size_t forward_prefix_len;

    (void)state;

    forward_prefix_len = strlcpy(long_forward_rule, "TCP=8023:10.0.2.15:",
                                 sizeof(long_forward_rule));
    assert_true(forward_prefix_len < sizeof(long_forward_rule));
    memset(long_forward_rule + forward_prefix_len, '1',
           sizeof(long_forward_rule) - forward_prefix_len - 1);
    long_forward_rule[sizeof(long_forward_rule) - 1] = '\0';

    assert_parse_fails("UNKNOWN=1", "Unexpected NAT argument: UNKNOWN");
    assert_parse_fails("GATEWAY=10.0.2.1/33", "Invalid network mask length");
    assert_parse_fails("GATEWAY=999.0.2.1", "Invalid host");
    assert_parse_fails("IPV6PREFIX=fd00::/127", "Invalid IPv6 prefix length");
    assert_parse_fails("IPV6PREFIX=not-ipv6/64", "Invalid IPv6 prefix");
    assert_parse_fails("IPV6GATEWAY=not-ipv6", "Invalid IPv6 gateway");
    assert_parse_fails("IPV6DNS=not-ipv6", "Invalid IPv6 nameserver");
    assert_parse_fails("DHCP=bad", "Invalid DHCP start address");
    assert_parse_fails("TCP=0:10.0.2.15:23", "Invalid port number");
    assert_parse_fails("UDP=8053:bad:53", "Invalid redirect IP address");
    assert_parse_fails(long_forward_rule, "NAT argument too long");
}

/* Verify open passes parsed config and forwarding rules to the backend. */
static void test_slirp_open_configures_backend_and_forwards(void **state)
{
    char errbuf[256];
    sim_slirp_handle *slirp;

    (void)state;

    reset_fake_backend();
    errbuf[0] = '\0';
    slirp =
        sim_slirp_open("GATEWAY=192.0.2.1/25,"
                       "TCP=8023:192.0.2.15:23,"
                       "UDP=8053:192.0.2.15:53",
                       NULL, capture_packet, NULL, 0, errbuf, sizeof(errbuf));
    assert_non_null(slirp);
    assert_string_equal(errbuf, "");
    assert_int_equal(fake_backend_state.open_count, 1);
    assert_ipv4_equal(fake_backend_state.open_config.gateway, "192.0.2.1");
    assert_ipv4_equal(fake_backend_state.open_config.network, "192.0.2.0");
    assert_ipv4_equal(fake_backend_state.open_config.netmask,
                      "255.255.255.128");
    assert_int_equal(fake_backend_state.open_config.ipv6_enabled, 1);
    assert_ipv6_equal(fake_backend_state.open_config.ipv6_gateway, "fd00::2");

    assert_int_equal(fake_backend_state.add_count, 2);
    assert_int_equal(fake_backend_state.add_calls[0].protocol,
                     sim_slirp_forward_tcp);
    assert_int_equal(fake_backend_state.add_calls[0].host_port, 8023);
    assert_ipv4_equal(fake_backend_state.add_calls[0].guest_addr, "192.0.2.15");
    assert_int_equal(fake_backend_state.add_calls[0].guest_port, 23);
    assert_int_equal(fake_backend_state.add_calls[1].protocol,
                     sim_slirp_forward_udp);
    assert_int_equal(fake_backend_state.add_calls[1].host_port, 8053);
    assert_ipv4_equal(fake_backend_state.add_calls[1].guest_addr, "192.0.2.15");
    assert_int_equal(fake_backend_state.add_calls[1].guest_port, 53);

    sim_slirp_close(slirp);
    assert_int_equal(fake_backend_state.close_count, 1);
    assert_int_equal(fake_backend_state.remove_count, 2);
    assert_int_equal(fake_backend_state.remove_calls[0].protocol,
                     sim_slirp_forward_udp);
    assert_int_equal(fake_backend_state.remove_calls[0].host_port, 8053);
    assert_int_equal(fake_backend_state.remove_calls[1].protocol,
                     sim_slirp_forward_tcp);
    assert_int_equal(fake_backend_state.remove_calls[1].host_port, 8023);
}

/* Verify select asks the backend for poll interests. */
static void test_slirp_select_uses_backend_poll_fill(void **state)
{
    char errbuf[256];
    sim_slirp_handle *slirp;

    (void)state;

    reset_fake_backend();
    errbuf[0] = '\0';
    slirp = sim_slirp_open("", NULL, capture_packet, NULL, 0, errbuf,
                           sizeof(errbuf));
    assert_non_null(slirp);

#if defined(_WIN32)
    /* Winsock select() requires at least one socket, unlike POSIX select(). */
    assert_int_equal(sim_slirp_select(slirp, 0), 0);
#else
    assert_int_equal(sim_slirp_select(slirp, 0), 1);
#endif
    assert_int_equal(fake_backend_state.fill_count, 1);

    sim_slirp_close(slirp);
}

/* Verify dispatch passes queued guest packets to the backend. */
static void test_slirp_dispatch_sends_queued_packets_to_backend(void **state)
{
    static const uint8_t packet[] = {0x01, 0x02, 0x03, 0x04};
    char errbuf[256];
    sim_slirp_handle *slirp;

    (void)state;

    reset_fake_backend();
    errbuf[0] = '\0';
    slirp = sim_slirp_open("", NULL, capture_packet, NULL, 0, errbuf,
                           sizeof(errbuf));
    assert_non_null(slirp);

    assert_int_equal(
        sim_slirp_send(slirp, (const char *)packet, sizeof(packet), 0),
        sizeof(packet));
    sim_slirp_dispatch(slirp);
    assert_int_equal(fake_backend_state.input_count, 1);
    assert_int_equal(fake_backend_state.last_input_size, sizeof(packet));
    assert_memory_equal(fake_backend_state.last_input, packet, sizeof(packet));
    assert_int_equal(fake_backend_state.poll_count, 1);

    sim_slirp_close(slirp);
}

/* Verify backend output packets reach the simulator callback. */
static void test_slirp_backend_output_reaches_packet_callback(void **state)
{
    static const uint8_t packet[] = {0xaa, 0xbb, 0xcc};
    struct callback_capture capture;
    char errbuf[256];
    sim_slirp_handle *slirp;

    (void)state;

    reset_fake_backend();
    memset(&capture, 0, sizeof(capture));
    errbuf[0] = '\0';
    slirp = sim_slirp_open("", &capture, capture_packet, NULL, 0, errbuf,
                           sizeof(errbuf));
    assert_non_null(slirp);

    sim_slirp_deliver_packet_for_test(slirp, packet, sizeof(packet));
    assert_int_equal(capture.packet_count, 1);
    assert_int_equal(capture.last_packet_size, sizeof(packet));
    assert_memory_equal(capture.last_packet, packet, sizeof(packet));

    sim_slirp_close(slirp);
}

/* Verify the production callback table covers the compiled libslirp ABI. */
static void test_slirp_real_backend_callbacks_match_libslirp_abi(void **state)
{
    (void)state;

    assert_true(sim_slirp_callbacks_are_complete_for_test());
}

/* Verify libslirp answers ARP for the virtual gateway without host network. */
static void test_slirp_real_backend_answers_local_arp(void **state)
{
    static const uint8_t guest_mac[ether_addr_len] = {0x52, 0x54, 0x00,
                                                    0x12, 0x34, 0x56};
    const sim_slirp_backend *previous;
    struct callback_capture capture;
    uint8_t request[42];
    char errbuf[256];
    sim_slirp_handle *slirp;
    int previous_doorbell;

    (void)state;

    memset(&capture, 0, sizeof(capture));
    build_arp_request(request, guest_mac, "10.0.2.15", "10.0.2.2");

    previous_doorbell = sim_slirp_set_doorbell_enabled_for_test(0);
    previous = sim_slirp_set_backend_for_test(NULL);
    errbuf[0] = '\0';
    slirp = sim_slirp_open("", &capture, capture_packet, NULL, 0, errbuf,
                           sizeof(errbuf));
    sim_slirp_set_backend_for_test(previous);
    sim_slirp_set_doorbell_enabled_for_test(previous_doorbell);

    assert_non_null(slirp);
    assert_string_equal(errbuf, "");
    assert_int_equal(
        sim_slirp_send(slirp, (const char *)request, sizeof(request), 0),
        sizeof(request));
    sim_slirp_dispatch(slirp);

    assert_int_equal(capture.packet_count, 1);
    assert_true(capture.last_packet_size >= 42);
    assert_memory_equal(capture.last_packet, guest_mac, ether_addr_len);
    assert_int_equal(get_be16(capture.last_packet + 12), ether_type_arp);
    assert_int_equal(get_be16(capture.last_packet + 20), arp_reply);
    assert_memory_equal(capture.last_packet + 28, request + 38, 4);
    assert_memory_equal(capture.last_packet + 32, guest_mac, ether_addr_len);
    assert_memory_equal(capture.last_packet + 38, request + 28, 4);

    sim_slirp_close(slirp);
}

/* Verify libslirp offers the configured DHCP lease and options. */
static void test_slirp_real_backend_answers_dhcp_discover(void **state)
{
    static const uint8_t guest_mac[ether_addr_len] = {0x52, 0x54, 0x00,
                                                    0xab, 0xcd, 0xef};
    const sim_slirp_backend *previous;
    const uint32_t xid = 0x12345678;
    struct callback_capture capture;
    uint8_t request[300];
    const uint8_t *option;
    size_t option_len;
    size_t request_len;
    char errbuf[256];
    sim_slirp_handle *slirp;
    int previous_doorbell;

    (void)state;

    memset(&capture, 0, sizeof(capture));
    request_len = build_dhcp_discover(request, guest_mac, xid);

    previous_doorbell = sim_slirp_set_doorbell_enabled_for_test(0);
    previous = sim_slirp_set_backend_for_test(NULL);
    errbuf[0] = '\0';
    slirp = sim_slirp_open("", &capture, capture_packet, NULL, 0, errbuf,
                           sizeof(errbuf));
    sim_slirp_set_backend_for_test(previous);
    sim_slirp_set_doorbell_enabled_for_test(previous_doorbell);

    assert_non_null(slirp);
    assert_string_equal(errbuf, "");
    assert_int_equal(
        sim_slirp_send(slirp, (const char *)request, request_len, 0),
        request_len);
    sim_slirp_dispatch(slirp);

    assert_int_equal(capture.packet_count, 1);
    assert_true(capture.last_packet_size >= dhcp_options_offset + 4);
    assert_ether_dest_is_guest_or_broadcast(capture.last_packet, guest_mac);
    assert_int_equal(get_be16(capture.last_packet + 12), ether_type_ipv4);
    assert_int_equal(capture.last_packet[23], ipv4_proto_udp);
    assert_int_equal(get_be16(capture.last_packet + 34), dhcp_server_port);
    assert_int_equal(get_be16(capture.last_packet + 36), dhcp_client_port);
    assert_int_equal(capture.last_packet[42], dhcp_bootreply);
    assert_int_equal(capture.last_packet[43], 1);
    assert_int_equal(capture.last_packet[44], ether_addr_len);
    assert_int_equal(get_be16(capture.last_packet + 46), (xid >> 16));
    assert_int_equal(get_be16(capture.last_packet + 48), (xid & 0xffff));
    assert_ipv4_bytes_equal(capture.last_packet + 58, "10.0.2.15");
    assert_memory_equal(capture.last_packet + 70, guest_mac, ether_addr_len);

    option =
        find_dhcp_option(capture.last_packet, (size_t)capture.last_packet_size,
                         dhcp_option_message_type, &option_len);
    assert_non_null(option);
    assert_int_equal(option_len, 1);
    assert_int_equal(option[0], dhcp_offer);

    option =
        find_dhcp_option(capture.last_packet, (size_t)capture.last_packet_size,
                         dhcp_option_server_id, &option_len);
    assert_non_null(option);
    assert_int_equal(option_len, 4);
    assert_ipv4_bytes_equal(option, "10.0.2.2");

    option =
        find_dhcp_option(capture.last_packet, (size_t)capture.last_packet_size,
                         dhcp_option_router, &option_len);
    assert_non_null(option);
    assert_int_equal(option_len, 4);
    assert_ipv4_bytes_equal(option, "10.0.2.2");

    option =
        find_dhcp_option(capture.last_packet, (size_t)capture.last_packet_size,
                         dhcp_option_dns, &option_len);
    assert_non_null(option);
    assert_int_equal(option_len, 4);
    assert_ipv4_bytes_equal(option, "10.0.2.3");

    sim_slirp_close(slirp);
}

/* Verify libslirp answers ICMP echo to the virtual gateway locally. */
static void test_slirp_real_backend_answers_gateway_ping(void **state)
{
    static const uint8_t guest_mac[ether_addr_len] = {0x52, 0x54, 0x00,
                                                    0x65, 0x43, 0x21};
    const sim_slirp_backend *previous;
    struct callback_capture capture;
    uint8_t host_mac[ether_addr_len];
    uint8_t packet[128];
    size_t packet_len;
    char errbuf[256];
    sim_slirp_handle *slirp;
    int previous_doorbell;

    (void)state;

    memset(&capture, 0, sizeof(capture));
    build_arp_request(packet, guest_mac, "10.0.2.15", "10.0.2.2");

    previous_doorbell = sim_slirp_set_doorbell_enabled_for_test(0);
    previous = sim_slirp_set_backend_for_test(NULL);
    errbuf[0] = '\0';
    slirp = sim_slirp_open("", &capture, capture_packet, NULL, 0, errbuf,
                           sizeof(errbuf));
    sim_slirp_set_backend_for_test(previous);
    sim_slirp_set_doorbell_enabled_for_test(previous_doorbell);

    assert_non_null(slirp);
    assert_string_equal(errbuf, "");
    assert_int_equal(sim_slirp_send(slirp, (const char *)packet, 42, 0), 42);
    sim_slirp_dispatch(slirp);

    assert_int_equal(capture.packet_count, 1);
    assert_int_equal(get_be16(capture.last_packet + 12), ether_type_arp);
    memcpy(host_mac, capture.last_packet + 6, ether_addr_len);

    memset(&capture, 0, sizeof(capture));
    packet_len = build_icmp_echo_request(packet, guest_mac, host_mac);
    assert_int_equal(sim_slirp_send(slirp, (const char *)packet, packet_len, 0),
                     packet_len);
    sim_slirp_dispatch(slirp);

    assert_int_equal(capture.packet_count, 1);
    assert_true(capture.last_packet_size >= 46);
    assert_memory_equal(capture.last_packet, guest_mac, ether_addr_len);
    assert_int_equal(get_be16(capture.last_packet + 12), ether_type_ipv4);
    assert_int_equal(capture.last_packet[23], ipv4_proto_icmp);
    assert_ipv4_bytes_equal(capture.last_packet + 26, "10.0.2.2");
    assert_ipv4_bytes_equal(capture.last_packet + 30, "10.0.2.15");
    assert_int_equal(capture.last_packet[34], icmp_echo_reply);
    assert_int_equal(get_be16(capture.last_packet + 38), 0x1234);
    assert_int_equal(get_be16(capture.last_packet + 40), 1);
    assert_memory_equal(capture.last_packet + 42, "zimh", 4);

    sim_slirp_close(slirp);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_slirp_parse_default_config),
        cmocka_unit_test(test_slirp_parse_gateway_and_network_config),
        cmocka_unit_test(test_slirp_parse_adjusts_network_address_gateway),
        cmocka_unit_test(test_slirp_parse_ipv6_options),
        cmocka_unit_test(test_slirp_parse_dhcp_options),
        cmocka_unit_test(test_slirp_parse_dns_tftp_and_boot_options),
        cmocka_unit_test(test_slirp_parse_forwarding_rules),
        cmocka_unit_test(test_slirp_parse_reports_missing_values),
        cmocka_unit_test(test_slirp_parse_reports_invalid_values),
        cmocka_unit_test(test_slirp_open_configures_backend_and_forwards),
        cmocka_unit_test(test_slirp_select_uses_backend_poll_fill),
        cmocka_unit_test(test_slirp_dispatch_sends_queued_packets_to_backend),
        cmocka_unit_test(test_slirp_backend_output_reaches_packet_callback),
        cmocka_unit_test(test_slirp_real_backend_callbacks_match_libslirp_abi),
        cmocka_unit_test(test_slirp_real_backend_answers_local_arp),
        cmocka_unit_test(test_slirp_real_backend_answers_dhcp_discover),
        cmocka_unit_test(test_slirp_real_backend_answers_gateway_ping),
    };

    return cmocka_run_group_tests(tests, setup_slirp_tests,
                                  teardown_slirp_tests);
}
