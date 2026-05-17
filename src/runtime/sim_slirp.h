/* sim_slirp.h: simulator NAT networking interface */
// SPDX-FileCopyrightText: 2015 Mark Pizzolato
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: X11

#ifndef SIM_SLIRP_H
#define SIM_SLIRP_H

#if defined(HAVE_SLIRP_NETWORK)
#include <stdint.h>

#include "sim_defs.h"
#include "sim_sock.h"
#if defined(_WIN32)
#include <ws2tcpip.h>
#endif
#include "sim_types.h"

/* Protocol values used by parsed host-forwarding rules. */
typedef enum sim_slirp_forward_protocol {
    sim_slirp_forward_tcp = 0,
    sim_slirp_forward_udp = 1
} sim_slirp_forward_protocol;

/* One parsed TCP= or UDP= host-to-guest forwarding rule. */
typedef struct sim_slirp_forward_rule {
    struct in_addr guest_addr;           /* guest address to forward to */
    sim_slirp_forward_protocol protocol; /* TCP or UDP forwarding */
    int host_port;                       /* host-side listening port */
    int guest_port;                      /* guest-side destination port */
    struct sim_slirp_forward_rule *next; /* next rule in argument order */
} sim_slirp_forward_rule;

/* Structured representation of a parsed NAT attachment option string. */
typedef struct sim_slirp_config {
    struct in_addr network;                /* guest virtual network */
    struct in_addr netmask;                /* guest virtual netmask */
    int maskbits;                          /* CIDR prefix length */
    struct in_addr gateway;                /* guest-visible host address */
    int ipv6_enabled;                      /* nonzero when IPv6 is enabled */
    struct in6_addr ipv6_prefix;           /* guest virtual IPv6 prefix */
    int ipv6_prefix_len;                   /* IPv6 prefix length */
    struct in6_addr ipv6_gateway;          /* guest-visible IPv6 host address */
    struct in6_addr ipv6_nameserver;       /* guest-visible IPv6 DNS address */
    int dhcp_enabled;                      /* nonzero when DHCP is enabled */
    struct in_addr dhcp_start;             /* first DHCP lease address */
    struct in_addr nameserver;             /* guest-visible DNS address */
    char *boot_file;                       /* DHCP boot filename */
    char *tftp_path;                       /* TFTP root path */
    char *dns_search;                      /* owned DNS search string */
    char **dns_search_domains;             /* pointers into dns_search */
    sim_slirp_forward_rule *forward_rules; /* parsed TCP/UDP forwards */
} sim_slirp_config;

/* Opaque NAT adapter instance used by sim_ether.c. */
typedef struct sim_slirp sim_slirp_handle;

/* Backend operations implemented by libslirp or tests. */
typedef struct sim_slirp_backend {
    void *(*open)(const sim_slirp_config *config, void *opaque);
    void (*close)(void *state);
    int (*add_hostfwd)(void *state, int is_udp, struct in_addr host_addr,
                       int host_port, struct in_addr guest_addr,
                       int guest_port);
    int (*remove_hostfwd)(void *state, int is_udp, struct in_addr host_addr,
                          int host_port);
    void (*input)(void *state, const uint8_t *packet, int packet_size);
    void (*pollfds_fill)(void *state, void *pollfds, uint32_t *timeout);
    void (*pollfds_poll)(void *state, void *pollfds, int select_error);
    void (*connection_info)(void *state, FILE *st);
    int uses_doorbell; /* nonzero when sends need a local wakeup socket */
} sim_slirp_backend;

/* Callback used by a backend to deliver Ethernet frames to the simulator. */
typedef void (*packet_callback)(void *opaque, const uchar_t *buf,
                                int len);

/* Initialize a NAT configuration with historical SIMH defaults. */
void sim_slirp_config_init(sim_slirp_config *config);

/* Release all dynamic fields owned by a NAT configuration. */
void sim_slirp_config_free(sim_slirp_config *config);

/* Parse a comma-separated NAT option string into structured config data. */
int sim_slirp_config_parse(sim_slirp_config *config, const char *args,
                           char *errbuf, size_t errbuf_size);

/* Replace the backend implementation while running adapter unit tests. */
const sim_slirp_backend *sim_slirp_set_backend_for_test(
    const sim_slirp_backend *backend);

/* Enable or disable the local wakeup socket while running unit tests. */
int sim_slirp_set_doorbell_enabled_for_test(int enabled);

/* Deliver a backend output frame directly to the simulator callback. */
void sim_slirp_deliver_packet_for_test(sim_slirp_handle *slirp,
                                       const uint8_t *packet, int packet_size);

/* Verify the libslirp callback table matches the compiled libslirp ABI. */
int sim_slirp_callbacks_are_complete_for_test(void);

/* Open a NAT adapter using the supplied option string and packet callback. */
sim_slirp_handle *sim_slirp_open(const char *args, void *opaque,
                                 packet_callback callback, DEVICE *dptr,
                                 uint32_t dbit, char *errbuf, size_t errbuf_size);

/* Close a NAT adapter and release all owned resources. */
void sim_slirp_close(sim_slirp_handle *slirp);

/* Queue an Ethernet frame from the simulated guest for backend processing. */
int sim_slirp_send(sim_slirp_handle *slirp, const char *msg, size_t len,
                   int flags);

/* Poll backend sockets and wakeup state for pending NAT work. */
int sim_slirp_select(sim_slirp_handle *slirp, int ms_timeout);

/* Deliver queued guest frames and process backend completions. */
void sim_slirp_dispatch(sim_slirp_handle *slirp);

/* Print attach help for supported NAT options. */
t_stat sim_slirp_attach_help(FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag,
                             const char *cptr);

/* Print current NAT configuration and backend connection state. */
void sim_slirp_show(sim_slirp_handle *slirp, FILE *st);

#endif /* HAVE_SLIRP_NETWORK */

#endif
