/* sim_slirp.c: zimh adapter for upstream libslirp NAT networking */
// SPDX-FileCopyrightText: 2015 Mark Pizzolato
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: X11

/*
   This adapter preserves the SIMH-facing NAT API originally provided by
   Mark Pizzolato's bundled SLiRP glue while mapping it to upstream
   libslirp's public API.
*/

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <sys/types.h>
#endif

#include <glib.h>
#include <slirp/libslirp.h>

#include "sim_defs.h"
#include "sim_slirp.h"
#include "sim_sock.h"
#include "sim_timer.h"
#include "sim_types.h"

#if !defined(USE_READER_THREAD)
#undef pthread_mutex_init
#undef pthread_mutex_destroy
#undef pthread_mutex_lock
#undef pthread_mutex_unlock
#define pthread_mutex_init(mtx, val)
#define pthread_mutex_destroy(mtx)
#define pthread_mutex_lock(mtx)
#define pthread_mutex_unlock(mtx)
#define pthread_mutex_t int
#endif

/* libslirp 4.7 lacks these typedefs, but our adapter still supports it. */
#if SLIRP_CONFIG_VERSION_MAX >= 5
typedef slirp_ssize_t sim_slirp_packet_result;
#else
typedef ssize_t sim_slirp_packet_result;
#endif

#if SLIRP_CONFIG_VERSION_MAX >= 6
typedef slirp_os_socket sim_slirp_socket;
#else
typedef int sim_slirp_socket;
#endif

/* Display names indexed by sim_slirp_forward_protocol values. */
static const char default_ip_addr[] = "10.0.2.2";
static const char default_ipv6_prefix[] = "fd00::";
static const char default_ipv6_gateway[] = "fd00::2";
static const char default_ipv6_nameserver[] = "fd00::3";
static const char *forward_protocol_names[] = {"TCP", "UDP"};

/* Report only non-retryable doorbell socket failures as fatal. */
static int doorbell_error_is_fatal(void)
{
    int err = WSAGetLastError();

    if (err == WSAEACCES)
        return 1;
#if defined(EPERM)
    if (err == EPERM)
        return 1;
#endif
    return 0;
}

/* Queued Ethernet frame waiting to be delivered to libslirp. */
struct slirp_write_request {
    struct slirp_write_request *next; /* next queued or free request */
    char msg[1518];                   /* Ethernet frame buffer */
    size_t len;                       /* bytes valid in msg */
};

/* Poll record used to translate libslirp socket interests to select(). */
typedef struct sim_slirp_pollfd {
    sim_slirp_socket fd; /* socket supplied by libslirp */
    int events;         /* requested SLIRP_POLL_* events */
    int revents;        /* ready SLIRP_POLL_* events */
} sim_slirp_pollfd;

/* Timer record used for libslirp's timer callback interface. */
typedef struct sim_slirp_timer {
    struct sim_slirp_timer *next; /* next timer owned by the adapter */
    sim_slirp_handle *owner;      /* adapter instance that owns this timer */
    SlirpTimerId id;              /* libslirp timer identifier */
    void *cb_opaque;              /* callback value passed to libslirp */
    int active;                   /* nonzero when the timer is armed */
    int64_t expire_time_ms;       /* absolute expiration time in ms */
} sim_slirp_timer;

/* Complete state for one NAT attachment. */
struct sim_slirp {
    const sim_slirp_backend *backend; /* backend operation table */
    void *backend_state;              /* backend-private instance state */
    char *args;                       /* original NAT option string */
    sim_slirp_config config;          /* parsed NAT configuration */
    sim_slirp_pollfd *pollfds;        /* active backend socket interests */
    size_t poll_count;                /* entries valid in pollfds */
    size_t poll_capacity;             /* entries allocated in pollfds */
    sim_slirp_timer *timers;          /* active and inactive timers */
    SOCKET db_chime;                  /* write packet doorbell */
    struct slirp_write_request *write_requests; /* pending guest frames */
    struct slirp_write_request *write_buffers;  /* reusable frame buffers */
    pthread_mutex_t write_buffer_lock;          /* protects frame queues */
    void *opaque;             /* opaque value passed during packet delivery */
    packet_callback callback; /* backend-to-simulator packet callback */
    DEVICE *dptr;             /* device used for debug output */
    uint32_t dbit;            /* debug bit used for debug output */
};

/* Return a malloc-owned copy of text. */
static char *copy_string(const char *text)
{
    size_t len;
    char *copy;

    if (text == NULL)
        return NULL;
    len = strlen(text) + 1;
    copy = (char *)malloc(len);
    if (copy)
        memcpy(copy, text, len);
    return copy;
}

/* Store an error message when the caller supplied an error buffer. */
static void set_parse_error(char *errbuf, size_t errbuf_size,
                            const char *message)
{
    if (errbuf_size == 0)
        return;
    strlcpy(errbuf, message, errbuf_size);
}

/* Parse an IPv4 address and report the caller-supplied diagnostic on error. */
static int parse_ipv4(const char *text, struct in_addr *addr, char *errbuf,
                      size_t errbuf_size, const char *error)
{
    if ((text == NULL) || (*text == '\0') || (inet_aton(text, addr) == 0)) {
        set_parse_error(errbuf, errbuf_size, error);
        return -1;
    }
    return 0;
}

/* Parse a CIDR prefix length in the inclusive range 0..32. */
static int parse_maskbits(const char *text, int *maskbits, char *errbuf,
                          size_t errbuf_size)
{
    char *endptr;
    long parsed;

    if ((text == NULL) || (*text == '\0')) {
        set_parse_error(errbuf, errbuf_size, "Missing network mask length");
        return -1;
    }
    parsed = strtol(text, &endptr, 10);
    if ((*endptr != '\0') || (parsed < 0) || (parsed > 32)) {
        set_parse_error(errbuf, errbuf_size, "Invalid network mask length");
        return -1;
    }
    *maskbits = (int)parsed;
    return 0;
}

/* Parse an IPv6 prefix length in the inclusive range 0..126. */
static int parse_ipv6_prefix_len(const char *text, int *prefix_len,
                                 char *errbuf, size_t errbuf_size)
{
    char *endptr;
    long parsed;

    if ((text == NULL) || (*text == '\0')) {
        set_parse_error(errbuf, errbuf_size, "Missing IPv6 prefix length");
        return -1;
    }
    parsed = strtol(text, &endptr, 10);
    if ((*endptr != '\0') || (parsed < 0) || (parsed > 126)) {
        set_parse_error(errbuf, errbuf_size, "Invalid IPv6 prefix length");
        return -1;
    }
    *prefix_len = (int)parsed;
    return 0;
}

/* Convert a CIDR prefix length to a network-order IPv4 netmask. */
static struct in_addr netmask_from_bits(int maskbits)
{
    struct in_addr mask;

    if (maskbits == 0)
        mask.s_addr = 0;
    else
        mask.s_addr = htonl(0xFFFFFFFFu << (32 - maskbits));
    return mask;
}

/* Parse an IPv6 address and report the caller-supplied diagnostic on error. */
static int parse_ipv6(const char *text, struct in6_addr *addr, char *errbuf,
                      size_t errbuf_size, const char *error)
{
    if ((text == NULL) || (*text == '\0') ||
        (inet_pton(AF_INET6, text, addr) != 1)) {
        set_parse_error(errbuf, errbuf_size, error);
        return -1;
    }
    return 0;
}

/* Clear host bits beyond prefix_len in an IPv6 prefix. */
static void mask_ipv6_prefix(struct in6_addr *addr, int prefix_len)
{
    int byte = prefix_len / 8;
    int bits = prefix_len % 8;

    if (byte < 16) {
        if (bits != 0) {
            addr->s6_addr[byte] &= (uint8_t)(0xff << (8 - bits));
            ++byte;
        }
        while (byte < 16)
            addr->s6_addr[byte++] = 0;
    }
}

/* Derive small host IDs inside an IPv6 prefix. */
static struct in6_addr ipv6_addr_from_prefix(struct in6_addr prefix,
                                             int host_id)
{
    mask_ipv6_prefix(&prefix, 126);
    prefix.s6_addr[15] = (uint8_t)((prefix.s6_addr[15] & 0xfc) | host_id);
    return prefix;
}

/* Parse a TCP or UDP port number in the inclusive range 1..65535. */
static int parse_port(const char *text, int *port, char *errbuf,
                      size_t errbuf_size)
{
    char *endptr;
    long parsed;

    parsed = strtol(text, &endptr, 10);
    if ((*text == '\0') || (*endptr != '\0') || (parsed < 1) ||
        (parsed > 65535)) {
        set_parse_error(errbuf, errbuf_size, "Invalid port number");
        return -1;
    }
    *port = (int)parsed;
    return 0;
}

/* Parse one TCP= or UDP= host-forwarding rule. */
static int parse_forward_rule(sim_slirp_forward_rule **head, const char *buff,
                              int is_udp, char *errbuf, size_t errbuf_size)
{
    char gbuf[4 * CBUFSIZE];
    int port = 0;
    int lport = 0;
    char *ipaddrstr = NULL;
    char *portstr = NULL;
    sim_slirp_forward_rule *newp;

    strlcpy(gbuf, buff, sizeof(gbuf));
    if (((ipaddrstr = strchr(gbuf, ':')) == NULL) || (*(ipaddrstr + 1) == 0)) {
        snprintf(errbuf, errbuf_size, "redir %s syntax error",
                 forward_protocol_names[is_udp]);
        return -1;
    }
    *ipaddrstr++ = 0;

    if ((ipaddrstr) && (((portstr = strchr(ipaddrstr, ':')) == NULL) ||
                        (*(portstr + 1) == 0))) {
        snprintf(errbuf, errbuf_size, "redir %s syntax error",
                 forward_protocol_names[is_udp]);
        return -1;
    }
    *portstr++ = 0;

    if ((parse_port(gbuf, &lport, errbuf, errbuf_size) != 0) ||
        (parse_port(portstr, &port, errbuf, errbuf_size) != 0))
        return -1;
    if ((ipaddrstr == NULL) || (*ipaddrstr == '\0')) {
        snprintf(errbuf, errbuf_size,
                 "%s redirection error: an IP address must be specified",
                 forward_protocol_names[is_udp]);
        return -1;
    }

    newp = (sim_slirp_forward_rule *)malloc(sizeof(*newp));
    if (newp == NULL)
        return -1;
    if (parse_ipv4(ipaddrstr, &newp->guest_addr, errbuf, errbuf_size,
                   "Invalid redirect IP address") != 0) {
        free(newp);
        return -1;
    }
    newp->protocol = is_udp;
    newp->guest_port = port;
    newp->host_port = lport;
    newp->next = *head;
    *head = newp;
    return 0;
}

/* Initialize a NAT configuration with historical SIMH defaults. */
void sim_slirp_config_init(sim_slirp_config *config)
{
    memset(config, 0, sizeof(*config));
    config->maskbits = 24;
    config->ipv6_enabled = 1;
    config->ipv6_prefix_len = 64;
    config->dhcp_enabled = 1;
    inet_aton(default_ip_addr, &config->gateway);
    inet_pton(AF_INET6, default_ipv6_prefix, &config->ipv6_prefix);
    inet_pton(AF_INET6, default_ipv6_gateway, &config->ipv6_gateway);
    inet_pton(AF_INET6, default_ipv6_nameserver, &config->ipv6_nameserver);
}

/* Release all dynamic fields owned by a NAT configuration. */
void sim_slirp_config_free(sim_slirp_config *config)
{
    sim_slirp_forward_rule *rule;

    if (config == NULL)
        return;
    free(config->boot_file);
    free(config->tftp_path);
    free(config->dns_search);
    free(config->dns_search_domains);
    while ((rule = config->forward_rules) != NULL) {
        config->forward_rules = rule->next;
        free(rule);
    }
    sim_slirp_config_init(config);
}

/* Fill in derived network, mask, DHCP, and DNS defaults after parsing. */
static void finalize_config(sim_slirp_config *config, int network_set,
                            int ipv6_gateway_set, int ipv6_dns_set)
{
    config->netmask = netmask_from_bits(config->maskbits);
    if (!network_set)
        config->network.s_addr =
            config->gateway.s_addr & config->netmask.s_addr;
    if ((config->gateway.s_addr & ~config->netmask.s_addr) == 0)
        config->gateway.s_addr = htonl(ntohl(config->network.s_addr) | 2);
    if (config->dhcp_enabled) {
        if (config->dhcp_start.s_addr == 0)
            config->dhcp_start.s_addr =
                htonl(ntohl(config->network.s_addr) | 15);
    } else
        config->dhcp_start.s_addr = 0;
    if (config->nameserver.s_addr == 0)
        config->nameserver.s_addr = htonl(ntohl(config->network.s_addr) | 3);
    mask_ipv6_prefix(&config->ipv6_prefix, config->ipv6_prefix_len);
    if (!ipv6_gateway_set)
        config->ipv6_gateway = ipv6_addr_from_prefix(config->ipv6_prefix, 2);
    if (!ipv6_dns_set)
        config->ipv6_nameserver = ipv6_addr_from_prefix(config->ipv6_prefix, 3);
}

/* Parse a comma-separated NAT option string into structured config data. */
int sim_slirp_config_parse(sim_slirp_config *config, const char *args,
                           char *errbuf, size_t errbuf_size)
{
    char *targs = copy_string(args ? args : "");
    const char *tptr = targs;
    const char *cptr;
    char tbuf[CBUFSIZE], gbuf[CBUFSIZE], abuf[CBUFSIZE];
    int network_set = 0;
    int ipv6_gateway_set = 0;
    int ipv6_dns_set = 0;
    int err = 0;

    if (targs == NULL)
        return -1;
    while (*tptr && !err) {
        const char *comma = strchr(tptr, ',');
        size_t option_len = comma ? (size_t)(comma - tptr) : strlen(tptr);

        if (option_len >= sizeof(tbuf)) {
            set_parse_error(errbuf, errbuf_size, "NAT argument too long");
            err = 1;
            break;
        }
        tptr = get_glyph_nc(tptr, tbuf, ',');
        if (!tbuf[0])
            break;
        cptr = tbuf;
        cptr = get_glyph(cptr, gbuf, '=');
        if (0 == MATCH_CMD(gbuf, "DHCP")) {
            config->dhcp_enabled = 1;
            if (cptr && *cptr)
                err = parse_ipv4(cptr, &config->dhcp_start, errbuf, errbuf_size,
                                 "Invalid DHCP start address");
            continue;
        }
        if (0 == MATCH_CMD(gbuf, "TFTP")) {
            if (cptr && *cptr)
                config->tftp_path = copy_string(cptr);
            else {
                set_parse_error(errbuf, errbuf_size, "Missing TFTP Path");
                err = 1;
            }
            continue;
        }
        if (0 == MATCH_CMD(gbuf, "BOOTFILE")) {
            if (cptr && *cptr)
                config->boot_file = copy_string(cptr);
            else {
                set_parse_error(errbuf, errbuf_size,
                                "Missing DHCP Boot file name");
                err = 1;
            }
            continue;
        }
        if ((0 == MATCH_CMD(gbuf, "NAMESERVER")) ||
            (0 == MATCH_CMD(gbuf, "DNS"))) {
            if (cptr && *cptr)
                err = parse_ipv4(cptr, &config->nameserver, errbuf, errbuf_size,
                                 "Invalid nameserver");
            else {
                set_parse_error(errbuf, errbuf_size, "Missing nameserver");
                err = 1;
            }
            continue;
        }
        if (0 == MATCH_CMD(gbuf, "DNSSEARCH")) {
            if (cptr && *cptr) {
                int count = 0;
                char *name;

                config->dns_search = copy_string(cptr);
                name = config->dns_search;
                do {
                    ++count;
                    config->dns_search_domains =
                        (char **)realloc(config->dns_search_domains,
                                         (count + 1) * sizeof(char *));
                    config->dns_search_domains[count] = NULL;
                    config->dns_search_domains[count - 1] = name;
                    name = strchr(name, ':');
                    if (name) {
                        *name = '\0';
                        ++name;
                    }
                } while (name && *name);
            } else {
                set_parse_error(errbuf, errbuf_size, "Missing DNS search list");
                err = 1;
            }
            continue;
        }
        if (0 == MATCH_CMD(gbuf, "GATEWAY")) {
            if (cptr && *cptr) {
                cptr = get_glyph(cptr, abuf, '/');
                if (cptr && *cptr)
                    err = parse_maskbits(cptr, &config->maskbits, errbuf,
                                         errbuf_size);
                if (!err)
                    err = parse_ipv4(abuf, &config->gateway, errbuf,
                                     errbuf_size, "Invalid host");
            } else {
                set_parse_error(errbuf, errbuf_size, "Missing host");
                err = 1;
            }
            continue;
        }
        if (0 == MATCH_CMD(gbuf, "NETWORK")) {
            if (cptr && *cptr) {
                cptr = get_glyph(cptr, abuf, '/');
                if (cptr && *cptr)
                    err = parse_maskbits(cptr, &config->maskbits, errbuf,
                                         errbuf_size);
                if (!err) {
                    err = parse_ipv4(abuf, &config->network, errbuf,
                                     errbuf_size, "Invalid network");
                    network_set = 1;
                }
            } else {
                set_parse_error(errbuf, errbuf_size, "Missing network");
                err = 1;
            }
            continue;
        }
        if (0 == MATCH_CMD(gbuf, "IPV6")) {
            config->ipv6_enabled = 1;
            continue;
        }
        if (0 == MATCH_CMD(gbuf, "NOIPV6")) {
            config->ipv6_enabled = 0;
            continue;
        }
        if (0 == MATCH_CMD(gbuf, "IPV6PREFIX")) {
            if (cptr && *cptr) {
                cptr = get_glyph(cptr, abuf, '/');
                if (cptr && *cptr)
                    err = parse_ipv6_prefix_len(cptr, &config->ipv6_prefix_len,
                                                errbuf, errbuf_size);
                if (!err)
                    err = parse_ipv6(abuf, &config->ipv6_prefix, errbuf,
                                     errbuf_size, "Invalid IPv6 prefix");
            } else {
                set_parse_error(errbuf, errbuf_size, "Missing IPv6 prefix");
                err = 1;
            }
            continue;
        }
        if (0 == MATCH_CMD(gbuf, "IPV6GATEWAY")) {
            if (cptr && *cptr) {
                err = parse_ipv6(cptr, &config->ipv6_gateway, errbuf,
                                 errbuf_size, "Invalid IPv6 gateway");
                ipv6_gateway_set = 1;
            } else {
                set_parse_error(errbuf, errbuf_size, "Missing IPv6 gateway");
                err = 1;
            }
            continue;
        }
        if ((0 == MATCH_CMD(gbuf, "IPV6NAMESERVER")) ||
            (0 == MATCH_CMD(gbuf, "IPV6DNS"))) {
            if (cptr && *cptr) {
                err = parse_ipv6(cptr, &config->ipv6_nameserver, errbuf,
                                 errbuf_size, "Invalid IPv6 nameserver");
                ipv6_dns_set = 1;
            } else {
                set_parse_error(errbuf, errbuf_size, "Missing IPv6 nameserver");
                err = 1;
            }
            continue;
        }
        if (0 == MATCH_CMD(gbuf, "NODHCP")) {
            config->dhcp_enabled = 0;
            continue;
        }
        if (0 == MATCH_CMD(gbuf, "UDP")) {
            if (cptr && *cptr)
                err = parse_forward_rule(&config->forward_rules, cptr,
                                         sim_slirp_forward_udp, errbuf,
                                         errbuf_size);
            else {
                set_parse_error(errbuf, errbuf_size,
                                "Missing UDP port mapping");
                err = 1;
            }
            continue;
        }
        if (0 == MATCH_CMD(gbuf, "TCP")) {
            if (cptr && *cptr)
                err = parse_forward_rule(&config->forward_rules, cptr,
                                         sim_slirp_forward_tcp, errbuf,
                                         errbuf_size);
            else {
                set_parse_error(errbuf, errbuf_size,
                                "Missing TCP port mapping");
                err = 1;
            }
            continue;
        }
        snprintf(errbuf, errbuf_size, "Unexpected NAT argument: %s", gbuf);
        err = 1;
    }
    if (!err)
        finalize_config(config, network_set, ipv6_gateway_set, ipv6_dns_set);
    free(targs);
    return err ? -1 : 0;
}

/* Return libslirp's virtual clock in nanoseconds. */
static int64_t libslirp_clock_get_ns(void *opaque)
{
    (void)opaque;
    return (int64_t)sim_os_msec() * 1000000;
}

/* Deliver an Ethernet frame emitted by libslirp to the simulator callback. */
static sim_slirp_packet_result libslirp_send_packet(const void *buf,
                                                    size_t len, void *opaque)
{
    sim_slirp_handle *slirp = (sim_slirp_handle *)opaque;

    if ((slirp == NULL) || (slirp->callback == NULL))
        return (sim_slirp_packet_result)len;
    slirp->callback(slirp->opaque, (const uchar_t *)buf, (int)len);
    return (sim_slirp_packet_result)len;
}

/* Log libslirp guest-side protocol errors through the simulator debug path. */
static void libslirp_guest_error(const char *msg, void *opaque)
{
    sim_slirp_handle *slirp = (sim_slirp_handle *)opaque;

    if (slirp)
        sim_debug(slirp->dbit, slirp->dptr, "SLIRP guest error: %s\n", msg);
}

/* Wake the simulator poll loop when libslirp has new work pending. */
static void libslirp_notify(void *opaque)
{
    sim_slirp_handle *slirp = (sim_slirp_handle *)opaque;

    if ((slirp != NULL) && (slirp->db_chime != INVALID_SOCKET))
        sim_write_sock(slirp->db_chime, "", 0);
}

#if SLIRP_CONFIG_VERSION_MAX < 6
/* Accept legacy libslirp fd registrations.
   Select interests are polled later. */
static void libslirp_register_poll_fd(int fd, void *opaque)
{
    (void)fd;
    (void)opaque;
}

/* Accept legacy libslirp fd unregistrations; no registration is kept. */
static void libslirp_unregister_poll_fd(int fd, void *opaque)
{
    (void)fd;
    (void)opaque;
}
#endif

#if SLIRP_CONFIG_VERSION_MAX >= 6
/* Accept libslirp socket registrations; select interests are polled later. */
static void libslirp_register_poll_socket(slirp_os_socket socket, void *opaque)
{
    (void)socket;
    (void)opaque;
}

/* Accept libslirp socket unregistrations; no persistent registration is kept. */
static void libslirp_unregister_poll_socket(slirp_os_socket socket,
                                            void *opaque)
{
    (void)socket;
    (void)opaque;
}
#endif

/* Allocate a libslirp timer wrapper owned by the SLIRP adapter. */
static void *libslirp_timer_new(SlirpTimerId id, void *cb_opaque, void *opaque)
{
    sim_slirp_handle *slirp = (sim_slirp_handle *)opaque;
    sim_slirp_timer *timer = (sim_slirp_timer *)calloc(1, sizeof(*timer));

    if (timer == NULL)
        return NULL;
    timer->owner = slirp;
    timer->id = id;
    timer->cb_opaque = cb_opaque;
    timer->next = slirp->timers;
    slirp->timers = timer;
    return timer;
}

/* Remove and free a libslirp timer wrapper. */
static void libslirp_timer_free(void *timer_ptr, void *opaque)
{
    sim_slirp_handle *slirp = (sim_slirp_handle *)opaque;
    sim_slirp_timer *timer = (sim_slirp_timer *)timer_ptr;
    sim_slirp_timer **scan;

    if ((slirp == NULL) || (timer == NULL))
        return;
    for (scan = &slirp->timers; *scan != NULL; scan = &(*scan)->next) {
        if (*scan == timer) {
            *scan = timer->next;
            free(timer);
            return;
        }
    }
}

/* Arm or re-arm a libslirp timer using libslirp's millisecond deadline. */
static void libslirp_timer_mod(void *timer_ptr, int64_t expire_time,
                               void *opaque)
{
    sim_slirp_timer *timer = (sim_slirp_timer *)timer_ptr;

    (void)opaque;
    if (timer == NULL)
        return;
    timer->expire_time_ms = expire_time;
    timer->active = 1;
}

/* Run expired libslirp timers before or after poll work. */
static void run_due_timers(sim_slirp_handle *slirp)
{
    sim_slirp_timer *timer;
    int64_t now = (int64_t)sim_os_msec();

    for (timer = slirp->timers; timer != NULL; timer = timer->next) {
        if (timer->active && (timer->expire_time_ms <= now)) {
            timer->active = 0;
            slirp_handle_timer((Slirp *)slirp->backend_state, timer->id,
                               timer->cb_opaque);
        }
    }
}

/* Shorten the caller's select timeout to the next libslirp timer deadline. */
static void apply_timer_timeout(sim_slirp_handle *slirp, uint32_t *timeout)
{
    sim_slirp_timer *timer;
    int64_t now = (int64_t)sim_os_msec();

    for (timer = slirp->timers; timer != NULL; timer = timer->next) {
        if (timer->active) {
            uint32_t delay = 0;

            if (timer->expire_time_ms > now) {
                int64_t delta = timer->expire_time_ms - now;

                delay = (delta > UINT32_MAX) ? UINT32_MAX : (uint32_t)delta;
            }
            if (delay < *timeout)
                *timeout = delay;
        }
    }
}

/* Callback table passed to each upstream libslirp instance. */
static SlirpCb sim_slirp_callbacks = {
    .send_packet = libslirp_send_packet,
    .guest_error = libslirp_guest_error,
    .clock_get_ns = libslirp_clock_get_ns,
    .timer_free = libslirp_timer_free,
    .timer_mod = libslirp_timer_mod,
#if SLIRP_CONFIG_VERSION_MAX < 6
    .register_poll_fd = libslirp_register_poll_fd,
    .unregister_poll_fd = libslirp_unregister_poll_fd,
#endif
    .notify = libslirp_notify,
    .timer_new_opaque = libslirp_timer_new,
#if SLIRP_CONFIG_VERSION_MAX >= 6
    .register_poll_socket = libslirp_register_poll_socket,
    .unregister_poll_socket = libslirp_unregister_poll_socket,
#endif
};

/* Create the upstream libslirp instance from parsed simulator NAT options. */
static void *real_backend_open(const sim_slirp_config *config, void *opaque)
{
    SlirpConfig cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.version = SLIRP_CONFIG_VERSION_MAX;
    cfg.restricted = 0;
    cfg.in_enabled = true;
    cfg.vnetwork = config->network;
    cfg.vnetmask = config->netmask;
    cfg.vhost = config->gateway;
    cfg.in6_enabled = config->ipv6_enabled != 0;
    cfg.vprefix_addr6 = config->ipv6_prefix;
    cfg.vprefix_len = (uint8_t)config->ipv6_prefix_len;
    cfg.vhost6 = config->ipv6_gateway;
    cfg.tftp_path = config->tftp_path;
    cfg.bootfile = config->boot_file;
    cfg.vdhcp_start = config->dhcp_start;
    cfg.vnameserver = config->nameserver;
    cfg.vnameserver6 = config->ipv6_nameserver;
    cfg.vdnssearch = (const char **)config->dns_search_domains;
    cfg.disable_dhcp = !config->dhcp_enabled;
    return slirp_new(&cfg, &sim_slirp_callbacks, opaque);
}

/* Destroy an upstream libslirp instance. */
static void real_backend_close(void *state)
{
    slirp_cleanup((Slirp *)state);
}

/* Install a host-to-guest forwarding rule in upstream libslirp. */
static int real_backend_add_hostfwd(void *state, int is_udp,
                                    struct in_addr host_addr, int host_port,
                                    struct in_addr guest_addr, int guest_port)
{
    return slirp_add_hostfwd((Slirp *)state, is_udp, host_addr, host_port,
                             guest_addr, guest_port);
}

/* Remove a host-to-guest forwarding rule from upstream libslirp. */
static int real_backend_remove_hostfwd(void *state, int is_udp,
                                       struct in_addr host_addr, int host_port)
{
    return slirp_remove_hostfwd((Slirp *)state, is_udp, host_addr, host_port);
}

/* Pass a guest Ethernet frame into upstream libslirp. */
static void real_backend_input(void *state, const uint8_t *packet,
                               int packet_size)
{
    slirp_input((Slirp *)state, packet, packet_size);
}

#if SLIRP_CONFIG_VERSION_MAX >= 6
/* Record a libslirp socket interest and return its poll index. */
static int add_poll_socket(slirp_os_socket fd, int events, void *opaque)
{
    sim_slirp_handle *slirp = (sim_slirp_handle *)opaque;
    sim_slirp_pollfd *new_pollfds;

    if (slirp->poll_count == slirp->poll_capacity) {
        size_t capacity = slirp->poll_capacity ? slirp->poll_capacity * 2 : 8;

        new_pollfds = (sim_slirp_pollfd *)realloc(
            slirp->pollfds, capacity * sizeof(*new_pollfds));
        if (new_pollfds == NULL)
            return -1;
        slirp->pollfds = new_pollfds;
        slirp->poll_capacity = capacity;
    }
    slirp->pollfds[slirp->poll_count].fd = fd;
    slirp->pollfds[slirp->poll_count].events = events;
    slirp->pollfds[slirp->poll_count].revents = 0;
    return (int)slirp->poll_count++;
}
#else
/* Record a legacy libslirp fd interest and return its poll index. */
static int add_poll_fd(int fd, int events, void *opaque)
{
    sim_slirp_handle *slirp = (sim_slirp_handle *)opaque;
    sim_slirp_pollfd *new_pollfds;

    if (slirp->poll_count == slirp->poll_capacity) {
        size_t capacity = slirp->poll_capacity ? slirp->poll_capacity * 2 : 8;

        new_pollfds = (sim_slirp_pollfd *)realloc(
            slirp->pollfds, capacity * sizeof(*new_pollfds));
        if (new_pollfds == NULL)
            return -1;
        slirp->pollfds = new_pollfds;
        slirp->poll_capacity = capacity;
    }
    slirp->pollfds[slirp->poll_count].fd = fd;
    slirp->pollfds[slirp->poll_count].events = events;
    slirp->pollfds[slirp->poll_count].revents = 0;
    return (int)slirp->poll_count++;
}
#endif

/* Return readiness bits for the poll index previously handed to libslirp. */
static int get_revents(int idx, void *opaque)
{
    sim_slirp_handle *slirp = (sim_slirp_handle *)opaque;

    if ((idx < 0) || ((size_t)idx >= slirp->poll_count))
        return 0;
    return slirp->pollfds[idx].revents;
}

/* Ask upstream libslirp which host sockets and timers need attention. */
static void real_backend_pollfds_fill(void *state, void *pollfds,
                                      uint32_t *timeout)
{
    sim_slirp_handle *slirp = (sim_slirp_handle *)pollfds;

    slirp->poll_count = 0;
#if SLIRP_CONFIG_VERSION_MAX >= 6
    slirp_pollfds_fill_socket((Slirp *)state, timeout, add_poll_socket, slirp);
#else
    slirp_pollfds_fill((Slirp *)state, timeout, add_poll_fd, slirp);
#endif
}

/* Report host socket readiness back to upstream libslirp. */
static void real_backend_pollfds_poll(void *state, void *pollfds,
                                      int select_error)
{
    sim_slirp_handle *slirp = (sim_slirp_handle *)pollfds;

    slirp_pollfds_poll((Slirp *)state, select_error, get_revents, slirp);
}

/* Print upstream libslirp's connection table. */
static void real_backend_connection_info(void *state, FILE *st)
{
    char *info = slirp_connection_info((Slirp *)state);

    if (info) {
        fputs(info, st);
        g_free(info);
    }
}

/* Production backend that delegates to upstream libslirp. */
static const sim_slirp_backend sim_slirp_real_backend = {
    real_backend_open,
    real_backend_close,
    real_backend_add_hostfwd,
    real_backend_remove_hostfwd,
    real_backend_input,
    real_backend_pollfds_fill,
    real_backend_pollfds_poll,
    real_backend_connection_info,
    1};

/* Active backend operation table; tests may temporarily replace it. */
static const sim_slirp_backend *active_backend = &sim_slirp_real_backend;

/* Test seam for disabling the local wakeup socket in packet-only tests. */
static int sim_slirp_doorbell_enabled = 1;

/* Replace the backend implementation while running adapter unit tests. */
const sim_slirp_backend *sim_slirp_set_backend_for_test(
    const sim_slirp_backend *backend)
{
    const sim_slirp_backend *previous = active_backend;

    active_backend = backend ? backend : &sim_slirp_real_backend;
    return previous;
}

/* Enable or disable the local doorbell socket for deterministic tests. */
int sim_slirp_set_doorbell_enabled_for_test(int enabled)
{
    int previous = sim_slirp_doorbell_enabled;

    sim_slirp_doorbell_enabled = enabled;
    return previous;
}

/* Inject an outgoing backend packet into the simulator callback for tests. */
void sim_slirp_deliver_packet_for_test(sim_slirp_handle *slirp,
                                       const uint8_t *packet, int packet_size)
{
    if ((slirp != NULL) && (slirp->callback != NULL))
        slirp->callback(slirp->opaque, packet, packet_size);
}

/* Verify the callback table has every callback required by this ABI version. */
int sim_slirp_callbacks_are_complete_for_test(void)
{
    if ((sim_slirp_callbacks.send_packet == NULL) ||
        (sim_slirp_callbacks.guest_error == NULL) ||
        (sim_slirp_callbacks.clock_get_ns == NULL) ||
        (sim_slirp_callbacks.timer_free == NULL) ||
        (sim_slirp_callbacks.timer_mod == NULL) ||
        (sim_slirp_callbacks.notify == NULL))
        return 0;
#if SLIRP_CONFIG_VERSION_MAX < 6
    if ((sim_slirp_callbacks.register_poll_fd == NULL) ||
        (sim_slirp_callbacks.unregister_poll_fd == NULL))
        return 0;
#endif
#if SLIRP_CONFIG_VERSION_MAX >= 4
    if (sim_slirp_callbacks.timer_new_opaque == NULL)
        return 0;
#endif
#if SLIRP_CONFIG_VERSION_MAX >= 6
    if ((sim_slirp_callbacks.register_poll_socket == NULL) ||
        (sim_slirp_callbacks.unregister_poll_socket == NULL))
        return 0;
#endif
    return 1;
}

/* Apply parsed TCP/UDP host-forwarding rules in original argument order. */
static int apply_forward_rules(sim_slirp_handle *slirp,
                               sim_slirp_forward_rule *head)
{
    struct in_addr host_addr;
    int ret = 0;

    host_addr.s_addr = htonl(INADDR_ANY);
    if (head) {
        ret = apply_forward_rules(slirp, head->next);
        if (slirp->backend->add_hostfwd(
                slirp->backend_state, head->protocol, host_addr,
                head->host_port, head->guest_addr, head->guest_port) < 0) {
            sim_printf("Can't establish redirector for: redir %s   =%d:%s:%d\n",
                       forward_protocol_names[head->protocol], head->host_port,
                       inet_ntoa(head->guest_addr), head->guest_port);
            ++ret;
        }
    }
    return ret;
}

/* Open and configure a SLIRP NAT adapter instance. */
sim_slirp_handle *sim_slirp_open(const char *args, void *opaque,
                                 packet_callback callback, DEVICE *dptr,
                                 uint32_t dbit, char *errbuf, size_t errbuf_size)
{
    sim_slirp_handle *slirp = (sim_slirp_handle *)calloc(1, sizeof(*slirp));

    if (slirp == NULL)
        return NULL;
    slirp->backend = active_backend;
    slirp->args = copy_string(args ? args : "");
    slirp->opaque = opaque;
    slirp->callback = callback;
    slirp->db_chime = INVALID_SOCKET;
    slirp->dbit = dbit;
    slirp->dptr = dptr;
    sim_slirp_config_init(&slirp->config);
    pthread_mutex_init(&slirp->write_buffer_lock, NULL);

    if (sim_slirp_config_parse(&slirp->config, args, errbuf, errbuf_size) !=
        0) {
        sim_slirp_close(slirp);
        return NULL;
    }

    slirp->backend_state = slirp->backend->open(&slirp->config, slirp);
    if (slirp->backend_state == NULL) {
        sim_slirp_close(slirp);
        return NULL;
    }

    if (apply_forward_rules(slirp, slirp->config.forward_rules)) {
        sim_slirp_close(slirp);
        return NULL;
    }

    if (slirp->backend->uses_doorbell && sim_slirp_doorbell_enabled) {
        char db_host[32];
        int64_t rnd_val = (int64_t)sim_os_msec();
        int attempt;

        for (attempt = 0;
             (slirp->db_chime == INVALID_SOCKET) && (attempt < 65535);
             ++attempt) {
            int port = (int)((rnd_val + attempt) & 0xFFFF);

            if (port == 0)
                continue;
            snprintf(db_host, sizeof(db_host), "localhost:%d", port);
            slirp->db_chime = sim_connect_sock_ex(db_host, db_host, NULL, NULL,
                                                  SIM_SOCK_OPT_DATAGRAM |
                                                      SIM_SOCK_OPT_BLOCKING);
            if ((slirp->db_chime == INVALID_SOCKET) &&
                doorbell_error_is_fatal())
                break;
        }
        if (slirp->db_chime == INVALID_SOCKET) {
            sim_slirp_close(slirp);
            return NULL;
        }
    }

    sim_slirp_show(slirp, stdout);
    if (sim_log && (sim_log != stdout))
        sim_slirp_show(slirp, sim_log);
    if (sim_deb && (sim_deb != stdout) && (sim_deb != sim_log))
        sim_slirp_show(slirp, sim_deb);
    return slirp;
}

/* Close a SLIRP NAT adapter instance and release all owned resources. */
void sim_slirp_close(sim_slirp_handle *slirp)
{
    sim_slirp_forward_rule *rtmp;
    struct in_addr host_addr;

    if (slirp) {
        free(slirp->args);
        host_addr.s_addr = htonl(INADDR_ANY);
        while ((rtmp = slirp->config.forward_rules)) {
            if (slirp->backend_state)
                slirp->backend->remove_hostfwd(slirp->backend_state,
                                               rtmp->protocol, host_addr,
                                               rtmp->host_port);
            slirp->config.forward_rules = rtmp->next;
            free(rtmp);
        }
        sim_slirp_config_free(&slirp->config);
        if (slirp->db_chime != INVALID_SOCKET)
            closesocket(slirp->db_chime);
        if (1) {
            struct slirp_write_request *buffer;

            while (NULL != (buffer = slirp->write_buffers)) {
                slirp->write_buffers = buffer->next;
                free(buffer);
            }
            while (NULL != (buffer = slirp->write_requests)) {
                slirp->write_requests = buffer->next;
                free(buffer);
            }
        }
        pthread_mutex_destroy(&slirp->write_buffer_lock);
        if (slirp->backend_state)
            slirp->backend->close(slirp->backend_state);
        while (slirp->timers)
            libslirp_timer_free(slirp->timers, slirp);
        free(slirp->pollfds);
    }
    free(slirp);
}

/* Print simulator help for supported NAT options. */
t_stat sim_slirp_attach_help(FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag,
                             const char *cptr)
{
    (void)dptr;
    (void)uptr;
    (void)flag;
    (void)cptr;
    fprintf(
        st, "%s",
        "NAT options:\n"
        "    DHCP{=dhcp_start_address}           Enables DHCP server and "
        "specifies\n"
        "                                        guest LAN DHCP start IP "
        "address\n"
        "    BOOTFILE=bootfilename               specifies DHCP returned Boot "
        "Filename\n"
        "    TFTP=tftp-base-path                 Enables TFTP server and "
        "specifies\n"
        "                                        base file path\n"
        "    NAMESERVER=nameserver_ipaddres      specifies DHCP nameserver IP "
        "address\n"
        "    DNS=nameserver_ipaddres             specifies DHCP nameserver IP "
        "address\n"
        "    DNSSEARCH=domain{:domain{:domain}}  specifies DNS Domains search "
        "suffixes\n"
        "    GATEWAY=host_ipaddress{/masklen}    specifies LAN gateway IP "
        "address\n"
        "    NETWORK=network_ipaddress{/masklen} specifies LAN network "
        "address\n"
        "    IPV6                                enables IPv6\n"
        "    NOIPV6                              disables IPv6\n"
        "    IPV6PREFIX=prefix{/prefixlen}       specifies IPv6 network "
        "prefix\n"
        "    IPV6GATEWAY=host_ipv6address        specifies IPv6 gateway\n"
        "    IPV6DNS=nameserver_ipv6address      specifies IPv6 DNS "
        "address\n"
        "    IPV6NAMESERVER=nameserver_ipv6address specifies IPv6 DNS "
        "address\n"
        "    UDP=port:address:address's-port     maps host UDP port to guest "
        "port\n"
        "    TCP=port:address:address's-port     maps host TCP port to guest "
        "port\n"
        "    NODHCP                              disables DHCP server\n\n"
        "Default NAT Options: GATEWAY=10.0.2.2, masklen=24(netmask is "
        "255.255.255.0)\n"
        "                     DHCP=10.0.2.15, NAMESERVER=10.0.2.3\n"
        "                     IPV6PREFIX=fd00::/64, IPV6GATEWAY=fd00::2, "
        "IPV6DNS=fd00::3\n"
        "    Nameserver defaults to proxy traffic to host system's active "
        "nameserver\n\n");
    return SCPE_OK;
}

/* Queue a guest Ethernet frame for delivery to libslirp. */
int sim_slirp_send(sim_slirp_handle *slirp, const char *msg, size_t len,
                   int flags)
{
    struct slirp_write_request *request;
    int wake_needed = 0;

    (void)flags;
    if (!slirp) {
        errno = EBADF;
        return 0;
    }
    pthread_mutex_lock(&slirp->write_buffer_lock);
    if (NULL != (request = slirp->write_buffers))
        slirp->write_buffers = request->next;
    pthread_mutex_unlock(&slirp->write_buffer_lock);
    if (NULL == request)
        request = (struct slirp_write_request *)malloc(sizeof(*request));
    if (request == NULL)
        return 0;

    request->len = len;
    memcpy(request->msg, msg, len);

    pthread_mutex_lock(&slirp->write_buffer_lock);
    request->next = NULL;
    if (slirp->write_requests) {
        struct slirp_write_request *last_request = slirp->write_requests;

        while (last_request->next)
            last_request = last_request->next;
        last_request->next = request;
    } else {
        slirp->write_requests = request;
        wake_needed = 1;
    }
    pthread_mutex_unlock(&slirp->write_buffer_lock);

    if (wake_needed && (slirp->db_chime != INVALID_SOCKET))
        sim_write_sock(slirp->db_chime, msg, 0);
    return (int)len;
}

/* Print the configured NAT state and backend connection information. */
void sim_slirp_show(sim_slirp_handle *slirp, FILE *st)
{
    sim_slirp_forward_rule *rtmp;

    if ((slirp == NULL) || (slirp->backend_state == NULL))
        return;
    fprintf(st, "NAT args: %s\n", slirp->args);
    fprintf(st, "NAT network setup:\n");
    fprintf(st, "        gateway       =%s/%d",
            inet_ntoa(slirp->config.gateway), slirp->config.maskbits);
    fprintf(st, "(%s)\n", inet_ntoa(slirp->config.netmask));
    fprintf(st, "        DNS           =%s\n",
            inet_ntoa(slirp->config.nameserver));
    if (slirp->config.dhcp_start.s_addr != 0)
        fprintf(st, "        dhcp_start    =%s\n",
                inet_ntoa(slirp->config.dhcp_start));
    if (slirp->config.ipv6_enabled) {
        char addrbuf[INET6_ADDRSTRLEN];

        inet_ntop(AF_INET6, &slirp->config.ipv6_prefix, addrbuf,
                  sizeof(addrbuf));
        fprintf(st, "        IPv6 prefix   =%s/%d\n", addrbuf,
                slirp->config.ipv6_prefix_len);
        inet_ntop(AF_INET6, &slirp->config.ipv6_gateway, addrbuf,
                  sizeof(addrbuf));
        fprintf(st, "        IPv6 gateway  =%s\n", addrbuf);
        inet_ntop(AF_INET6, &slirp->config.ipv6_nameserver, addrbuf,
                  sizeof(addrbuf));
        fprintf(st, "        IPv6 DNS      =%s\n", addrbuf);
    } else {
        fprintf(st, "        IPv6          =disabled\n");
    }
    if (slirp->config.boot_file)
        fprintf(st, "        dhcp bootfile =%s\n", slirp->config.boot_file);
    if (slirp->config.dns_search_domains) {
        char **domains = slirp->config.dns_search_domains;

        fprintf(st, "        DNS domains   =");
        while (*domains) {
            fprintf(st, "%s%s",
                    (domains != slirp->config.dns_search_domains) ? ", " : "",
                    *domains);
            ++domains;
        }
        fprintf(st, "\n");
    }
    if (slirp->config.tftp_path)
        fprintf(st, "        tftp prefix   =%s\n", slirp->config.tftp_path);
    rtmp = slirp->config.forward_rules;
    while (rtmp) {
        fprintf(st, "        redir %3s     =%d:%s:%d\n",
                forward_protocol_names[rtmp->protocol], rtmp->host_port,
                inet_ntoa(rtmp->guest_addr), rtmp->guest_port);
        rtmp = rtmp->next;
    }
    if (slirp->backend->connection_info)
        slirp->backend->connection_info(slirp->backend_state, st);
}

/* Convert remembered libslirp socket interests into select fd_sets. */
static int fill_select_fdsets(sim_slirp_handle *slirp, fd_set *rfds,
                              fd_set *wfds, fd_set *xfds)
{
    int nfds = -1;
    size_t i;

    for (i = 0; i < slirp->poll_count; i++) {
        sim_slirp_socket fd = slirp->pollfds[i].fd;
        int events = slirp->pollfds[i].events;

        if (events & SLIRP_POLL_IN) {
            FD_SET(fd, rfds);
            if ((int)fd > nfds)
                nfds = (int)fd;
        }
        if (events & SLIRP_POLL_OUT) {
            FD_SET(fd, wfds);
            if ((int)fd > nfds)
                nfds = (int)fd;
        }
        if (events & (SLIRP_POLL_PRI | SLIRP_POLL_HUP | SLIRP_POLL_ERR)) {
            FD_SET(fd, xfds);
            if ((int)fd > nfds)
                nfds = (int)fd;
        }
    }
    return nfds;
}

/* Convert select fd_sets back into revents for libslirp. */
static void collect_select_revents(sim_slirp_handle *slirp, fd_set *rfds,
                                   fd_set *wfds, fd_set *xfds)
{
    size_t i;

    for (i = 0; i < slirp->poll_count; i++) {
        sim_slirp_socket fd = slirp->pollfds[i].fd;
        int revents = 0;

        if (FD_ISSET(fd, rfds))
            revents |= SLIRP_POLL_IN;
        if (FD_ISSET(fd, wfds))
            revents |= SLIRP_POLL_OUT;
        if (FD_ISSET(fd, xfds))
            revents |= SLIRP_POLL_PRI;
        slirp->pollfds[i].revents = revents & slirp->pollfds[i].events;
    }
}

/* Poll libslirp sockets and the local doorbell for pending work. */
int sim_slirp_select(sim_slirp_handle *slirp, int ms_timeout)
{
    int select_ret;
    uint32_t slirp_timeout = (uint32_t)ms_timeout;
    struct timeval timeout;
    fd_set rfds, wfds, xfds;
    int nfds;

    if (!slirp)
        return -1;
    run_due_timers(slirp);
    slirp->backend->pollfds_fill(slirp->backend_state, slirp, &slirp_timeout);
    apply_timer_timeout(slirp, &slirp_timeout);
    timeout.tv_sec = slirp_timeout / 1000;
    timeout.tv_usec = (slirp_timeout % 1000) * 1000;

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&xfds);
    nfds = fill_select_fdsets(slirp, &rfds, &wfds, &xfds);
    if (slirp->db_chime != INVALID_SOCKET) {
        FD_SET(slirp->db_chime, &rfds);
        if ((int)slirp->db_chime > nfds)
            nfds = (int)slirp->db_chime;
    }
    select_ret = select(nfds + 1, &rfds, &wfds, &xfds, &timeout);
    if (select_ret) {
        if (select_ret > 0) {
            collect_select_revents(slirp, &rfds, &wfds, &xfds);
            if ((slirp->db_chime != INVALID_SOCKET) &&
                FD_ISSET(slirp->db_chime, &rfds)) {
                char buf[32];

                (void)recv(slirp->db_chime, buf, sizeof(buf), 0);
            }
        }
        sim_debug(slirp->dbit, slirp->dptr, "Select returned %d\r\n",
                  select_ret);
    }
    return select_ret + 1;
}

/* Deliver queued guest frames to libslirp and process backend completions. */
void sim_slirp_dispatch(sim_slirp_handle *slirp)
{
    struct slirp_write_request *request;

    run_due_timers(slirp);
    pthread_mutex_lock(&slirp->write_buffer_lock);
    while (NULL != (request = slirp->write_requests)) {
        slirp->write_requests = request->next;
        pthread_mutex_unlock(&slirp->write_buffer_lock);

        slirp->backend->input(slirp->backend_state, (const uint8_t *)request->msg,
                              (int)request->len);

        pthread_mutex_lock(&slirp->write_buffer_lock);
        request->next = slirp->write_buffers;
        slirp->write_buffers = request;
    }
    pthread_mutex_unlock(&slirp->write_buffer_lock);

    slirp->backend->pollfds_poll(slirp->backend_state, slirp, 0);
    run_due_timers(slirp);
}
