#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test_cmocka.h"

#include "scp.h"
#include "scp_context.h"
#include "sim_defs.h"
#include "sim_serial.h"
#include "sim_tmxr.h"
#include "sim_tmxr_internal.h"
#include "test_simh_personality.h"
#include "test_support.h"

struct sim_tmxr_fixture {
    DEVICE device;
    UNIT unit;
    UNIT line_units[4];
    UNIT out_units[4];
    TMXR mux;
    int32 lnorder[4];
    TMLN lines[4];
    struct {
        int master_sock_calls;
        SOCKET master_sock_result;
        t_stat master_sock_status;
        char last_master_sock_port[128];

        int accept_calls;
        SOCKET accept_results[4];
        char *accept_addresses[4];
        int accept_opt_flags[4];
        int next_accept_result;

        int connect_calls;
        SOCKET connect_result;
        char last_sourcehostport[128];
        char last_hostport[128];
        int last_connect_opt_flags;

        int check_conn_calls;
        SOCKET checked_socks[4];
        int checked_rd[4];
        int check_conn_results[4];
        int next_check_conn_result;

        int close_sock_calls;
        SOCKET closed_socks[4];

        int write_sock_calls;
        SOCKET written_socks[8];
        char write_messages[8][256];
        int write_lengths[8];

        int getnames_sock_calls;
        SOCKET named_socks[4];
        char sockname[128];
        char peername[128];

        int close_serial_calls;
        SERHANDLE closed_serial_ports[4];

        int write_serial_calls;
        SERHANDLE written_serial_ports[8];
        char serial_write_messages[8][256];
        int serial_write_lengths[8];

        int control_serial_calls;
        SERHANDLE control_serial_ports[4];
        int32 control_bits_set[4];
        int32 control_bits_clear[4];
        t_stat control_result;
        int32 control_status_bits;
        bool have_control_status_bits;

        int sleep_calls;
        unsigned int sleep_msec[4];
        uint32 sleep_result;

        int open_serial_calls;
        char last_open_serial_name[128];
        TMLN *last_open_serial_line;
        SERHANDLE open_serial_result;
        t_stat open_serial_status;

        int eth_devices_calls;
        int eth_devices_result;
        ETH_LIST eth_devices_list[4];

        int eth_open_calls;
        char last_eth_open_name[128];
        ETH_DEV *last_eth_open_dev;
        t_stat eth_open_result;

        int eth_read_calls;
        ETH_PACK eth_read_packets[4];
        int eth_read_results[4];
        int next_eth_read_result;

        int eth_write_calls;
        ETH_DEV *last_eth_write_dev;
        ETH_PACK eth_write_packets[4];
        t_stat eth_write_result;
        bool poll_rx_after_eth_write;

        int eth_filter_calls;
        ETH_DEV *last_eth_filter_dev;
        t_stat eth_filter_result;

        int eth_close_calls;
        ETH_DEV *last_eth_close_dev;
    } io;
};

static struct sim_tmxr_fixture *tmxr_io_fixture;

static SOCKET test_tmxr_master_sock(char *port, t_stat *status)
{
    assert_non_null(tmxr_io_fixture);
    tmxr_io_fixture->io.master_sock_calls++;
    snprintf(tmxr_io_fixture->io.last_master_sock_port,
             sizeof(tmxr_io_fixture->io.last_master_sock_port), "%s",
             port ? port : "");
    if (status != NULL)
        *status = tmxr_io_fixture->io.master_sock_status;
    return tmxr_io_fixture->io.master_sock_result;
}

static SOCKET test_tmxr_accept_conn_ex(SOCKET master, char **connectaddr,
                                       int opt_flags)
{
    int call;

    assert_non_null(tmxr_io_fixture);
    (void)master;
    call = tmxr_io_fixture->io.accept_calls;
    if (call < 4)
        tmxr_io_fixture->io.accept_opt_flags[call] = opt_flags;
    tmxr_io_fixture->io.accept_calls++;

    if (tmxr_io_fixture->io.next_accept_result >= 4)
        return INVALID_SOCKET;

    if (tmxr_io_fixture->io.next_accept_result >= 0) {
        int idx = tmxr_io_fixture->io.next_accept_result++;
        SOCKET result = tmxr_io_fixture->io.accept_results[idx];

        if (connectaddr != NULL) {
            if (tmxr_io_fixture->io.accept_addresses[idx] != NULL)
                *connectaddr = strdup(
                    tmxr_io_fixture->io.accept_addresses[idx]);
            else
                *connectaddr = NULL;
        }
        return result;
    }

    return INVALID_SOCKET;
}

static SOCKET test_tmxr_connect_sock_ex(const char *sourcehostport,
                                        const char *hostport,
                                        const char *default_host,
                                        const char *default_port,
                                        int opt_flags)
{
    (void)default_host;
    (void)default_port;

    assert_non_null(tmxr_io_fixture);
    tmxr_io_fixture->io.connect_calls++;
    snprintf(tmxr_io_fixture->io.last_sourcehostport,
             sizeof(tmxr_io_fixture->io.last_sourcehostport), "%s",
             sourcehostport ? sourcehostport : "");
    snprintf(tmxr_io_fixture->io.last_hostport,
             sizeof(tmxr_io_fixture->io.last_hostport), "%s",
             hostport ? hostport : "");
    tmxr_io_fixture->io.last_connect_opt_flags = opt_flags;
    return tmxr_io_fixture->io.connect_result;
}

static int test_tmxr_check_conn(SOCKET sock, int rd)
{
    int call;

    assert_non_null(tmxr_io_fixture);
    call = tmxr_io_fixture->io.check_conn_calls;
    if (call < 4) {
        tmxr_io_fixture->io.checked_socks[call] = sock;
        tmxr_io_fixture->io.checked_rd[call] = rd;
    }
    tmxr_io_fixture->io.check_conn_calls++;

    if (tmxr_io_fixture->io.next_check_conn_result >= 4)
        return 0;
    return tmxr_io_fixture
        ->io.check_conn_results[tmxr_io_fixture->io.next_check_conn_result++];
}

static void test_tmxr_close_sock(SOCKET sock)
{
    assert_non_null(tmxr_io_fixture);
    if (tmxr_io_fixture->io.close_sock_calls < 4)
        tmxr_io_fixture->io.closed_socks[tmxr_io_fixture->io.close_sock_calls] =
            sock;
    tmxr_io_fixture->io.close_sock_calls++;
}

static int test_tmxr_write_sock(SOCKET sock, const char *msg, int nbytes)
{
    int call;

    assert_non_null(tmxr_io_fixture);
    call = tmxr_io_fixture->io.write_sock_calls;
    if (call < 8) {
        tmxr_io_fixture->io.written_socks[call] = sock;
        tmxr_io_fixture->io.write_lengths[call] = nbytes;
        snprintf(tmxr_io_fixture->io.write_messages[call],
                 sizeof(tmxr_io_fixture->io.write_messages[call]), "%.*s",
                 nbytes, msg ? msg : "");
    }
    tmxr_io_fixture->io.write_sock_calls++;
    return nbytes;
}

static int test_tmxr_getnames_sock(SOCKET sock, char **socknamebuf,
                                   char **peernamebuf)
{
    assert_non_null(tmxr_io_fixture);
    if (tmxr_io_fixture->io.getnames_sock_calls < 4)
        tmxr_io_fixture->io.named_socks
            [tmxr_io_fixture->io.getnames_sock_calls] = sock;
    tmxr_io_fixture->io.getnames_sock_calls++;
    if (socknamebuf != NULL)
        *socknamebuf = strdup(tmxr_io_fixture->io.sockname);
    if (peernamebuf != NULL)
        *peernamebuf = strdup(tmxr_io_fixture->io.peername);
    return 0;
}

static void test_tmxr_close_serial(SERHANDLE port)
{
    assert_non_null(tmxr_io_fixture);
    if (tmxr_io_fixture->io.close_serial_calls < 4)
        tmxr_io_fixture->io.closed_serial_ports
            [tmxr_io_fixture->io.close_serial_calls] = port;
    tmxr_io_fixture->io.close_serial_calls++;
}

static int32 test_tmxr_write_serial(SERHANDLE port, char *buffer, int32 count)
{
    int call;

    assert_non_null(tmxr_io_fixture);
    call = tmxr_io_fixture->io.write_serial_calls;
    if (call < 8) {
        tmxr_io_fixture->io.written_serial_ports[call] = port;
        tmxr_io_fixture->io.serial_write_lengths[call] = count;
        snprintf(tmxr_io_fixture->io.serial_write_messages[call],
                 sizeof(tmxr_io_fixture->io.serial_write_messages[call]),
                 "%.*s", count, buffer ? buffer : "");
    }
    tmxr_io_fixture->io.write_serial_calls++;
    return count;
}

static t_stat test_tmxr_control_serial(SERHANDLE port, int32 bits_to_set,
                                       int32 bits_to_clear,
                                       int32 *incoming_bits)
{
    int call;

    assert_non_null(tmxr_io_fixture);
    call = tmxr_io_fixture->io.control_serial_calls;
    if (call < 4) {
        tmxr_io_fixture->io.control_serial_ports[call] = port;
        tmxr_io_fixture->io.control_bits_set[call] = bits_to_set;
        tmxr_io_fixture->io.control_bits_clear[call] = bits_to_clear;
    }
    tmxr_io_fixture->io.control_serial_calls++;
    if (incoming_bits != NULL && tmxr_io_fixture->io.have_control_status_bits)
        *incoming_bits = tmxr_io_fixture->io.control_status_bits;
    return tmxr_io_fixture->io.control_result;
}

static uint32 test_tmxr_ms_sleep(unsigned int msec)
{
    assert_non_null(tmxr_io_fixture);
    if (tmxr_io_fixture->io.sleep_calls < 4)
        tmxr_io_fixture->io.sleep_msec[tmxr_io_fixture->io.sleep_calls] = msec;
    tmxr_io_fixture->io.sleep_calls++;
    return tmxr_io_fixture->io.sleep_result;
}

static SERHANDLE test_tmxr_open_serial(char *name, TMLN *lp, t_stat *status)
{
    assert_non_null(tmxr_io_fixture);
    tmxr_io_fixture->io.open_serial_calls++;
    snprintf(tmxr_io_fixture->io.last_open_serial_name,
             sizeof(tmxr_io_fixture->io.last_open_serial_name), "%s",
             name ? name : "");
    tmxr_io_fixture->io.last_open_serial_line = lp;
    if (status != NULL)
        *status = tmxr_io_fixture->io.open_serial_status;
    return tmxr_io_fixture->io.open_serial_result;
}

static int test_tmxr_eth_devices(int max, ETH_LIST *dev, ETH_BOOL framers)
{
    int i;

    assert_non_null(tmxr_io_fixture);
    assert_true(framers);
    tmxr_io_fixture->io.eth_devices_calls++;
    assert_true(max >= tmxr_io_fixture->io.eth_devices_result);
    for (i = 0; i < tmxr_io_fixture->io.eth_devices_result; i++)
        dev[i] = tmxr_io_fixture->io.eth_devices_list[i];
    return tmxr_io_fixture->io.eth_devices_result;
}

static t_stat test_tmxr_eth_open(ETH_DEV *dev, const char *name, DEVICE *dptr,
                                 uint32 dbit)
{
    (void)dptr;
    (void)dbit;

    assert_non_null(tmxr_io_fixture);
    tmxr_io_fixture->io.eth_open_calls++;
    tmxr_io_fixture->io.last_eth_open_dev = dev;
    snprintf(tmxr_io_fixture->io.last_eth_open_name,
             sizeof(tmxr_io_fixture->io.last_eth_open_name), "%s",
             name ? name : "");
    return tmxr_io_fixture->io.eth_open_result;
}

static int test_tmxr_eth_read(ETH_DEV *dev, ETH_PACK *packet,
                              ETH_PCALLBACK routine)
{
    int result;

    assert_non_null(tmxr_io_fixture);
    (void)dev;
    (void)routine;

    tmxr_io_fixture->io.eth_read_calls++;
    if (tmxr_io_fixture->io.next_eth_read_result >= 4)
        return 0;

    result =
        tmxr_io_fixture->io
            .eth_read_results[tmxr_io_fixture->io.next_eth_read_result];
    if (result && packet != NULL)
        *packet =
            tmxr_io_fixture->io
                .eth_read_packets[tmxr_io_fixture->io.next_eth_read_result];
    tmxr_io_fixture->io.next_eth_read_result++;
    return result;
}

static t_stat test_tmxr_eth_write(ETH_DEV *dev, ETH_PACK *packet,
                                  ETH_PCALLBACK routine)
{
    int call;

    assert_non_null(tmxr_io_fixture);
    (void)routine;

    call = tmxr_io_fixture->io.eth_write_calls;
    tmxr_io_fixture->io.last_eth_write_dev = dev;
    if (call < 4 && packet != NULL)
        tmxr_io_fixture->io.eth_write_packets[call] = *packet;
    tmxr_io_fixture->io.eth_write_calls++;
    if (tmxr_io_fixture->io.poll_rx_after_eth_write) {
        tmxr_io_fixture->io.poll_rx_after_eth_write = false;
        tmxr_poll_rx(&tmxr_io_fixture->mux);
    }
    return tmxr_io_fixture->io.eth_write_result;
}

static t_stat test_tmxr_eth_filter(ETH_DEV *dev, int addr_count,
                                   const ETH_MAC addresses[],
                                   ETH_BOOL all_multicast,
                                   ETH_BOOL promiscuous)
{
    (void)addr_count;
    (void)addresses;
    (void)all_multicast;
    (void)promiscuous;

    assert_non_null(tmxr_io_fixture);
    tmxr_io_fixture->io.eth_filter_calls++;
    tmxr_io_fixture->io.last_eth_filter_dev = dev;
    return tmxr_io_fixture->io.eth_filter_result;
}

static t_stat test_tmxr_eth_close(ETH_DEV *dev)
{
    assert_non_null(tmxr_io_fixture);
    tmxr_io_fixture->io.eth_close_calls++;
    tmxr_io_fixture->io.last_eth_close_dev = dev;
    return SCPE_OK;
}

static void install_tmxr_test_io_hooks(void)
{
    static const TMXR_IO_HOOKS hooks = {
        test_tmxr_master_sock,
        test_tmxr_accept_conn_ex,
        test_tmxr_connect_sock_ex,
        test_tmxr_check_conn,
        test_tmxr_close_sock,
        test_tmxr_write_sock,
        test_tmxr_getnames_sock,
        test_tmxr_close_serial,
        test_tmxr_write_serial,
        test_tmxr_control_serial,
        test_tmxr_ms_sleep,
        test_tmxr_open_serial,
        test_tmxr_eth_devices,
        test_tmxr_eth_open,
        test_tmxr_eth_read,
        test_tmxr_eth_write,
        test_tmxr_eth_close,
        test_tmxr_eth_filter,
    };

    tmxr_set_io_hooks(&hooks);
}

static void ensure_line_tx_buffer(TMLN *line, int32 size)
{
    free(line->txb);
    line->txb = calloc((size_t)size, sizeof(*line->txb));
    assert_non_null(line->txb);
    line->txbsz = size;
    line->txbpr = 0;
    line->txbpi = 0;
    line->txcnt = 0;
    line->txdrp = 0;
    line->txstall = 0;
}

static char *copy_line_tx_buffer(const TMLN *line)
{
    int32 used = tmxr_tqln(line);
    char *copy = calloc((size_t)used + 1, sizeof(*copy));
    int32 i;

    assert_non_null(copy);
    for (i = 0; i < used; i++)
        copy[i] = line->txb[(line->txbpr + i) % line->txbsz];
    copy[used] = '\0';
    return copy;
}

static char *make_temp_log_path(void)
{
    char *path = strdup("/tmp/zimh-tmxr-log-XXXXXX");
    int fd;

    assert_non_null(path);
    fd = mkstemp(path);
    assert_true(fd >= 0);
    close(fd);
    unlink(path);
    return path;
}

static char *capture_tmxr_debug_output(uint32 dbits, TMLN *line,
                                       const char *msg, char *buf,
                                       int bufsize)
{
    FILE *saved_deb = sim_deb;
    int32 saved_deb_switches = sim_deb_switches;
    FILE *capture;
    char *output = NULL;
    size_t output_size = 0;

    capture = tmpfile();
    assert_non_null(capture);

    sim_deb = capture;
    sim_deb_switches |= SWMASK('F');
    _tmxr_debug(dbits, line, msg, buf, bufsize);
    assert_int_equal(fflush(capture), 0);
    assert_int_equal(
        simh_test_read_stream(capture, &output, &output_size), 0);
    assert_int_equal(fclose(capture), 0);
    sim_deb = saved_deb;
    sim_deb_switches = saved_deb_switches;

    return output;
}

static void set_test_framer_packet_header(ETH_PACK *packet, uint16 frame_len,
                                          uint8 first_payload)
{
    memset(packet, 0, sizeof(*packet));
    packet->len = 60;
    packet->msg[14] = (uint8)(frame_len & 0xFF);
    packet->msg[15] = (uint8)(frame_len >> 8);
    packet->msg[18] = first_payload;
}

static void set_test_framer_status_packet(ETH_PACK *packet, uint8 on_flags)
{
    set_test_framer_packet_header(packet, 4, 021);
    packet->msg[19] = on_flags;
}

static void register_and_open_test_tmxr(struct sim_tmxr_fixture *fixture)
{
    assert_int_equal(sim_register_internal_device(&fixture->device), SCPE_OK);
    assert_int_equal(tmxr_open_master(&fixture->mux, "BUFFERED"), SCPE_OK);
}

static int setup_sim_tmxr_fixture(void **state)
{
    struct sim_tmxr_fixture *fixture;
    int32 i;

    simh_test_reset_simulator_state();
    fixture = calloc(1, sizeof(*fixture));
    assert_non_null(fixture);

    memset(&fixture->device, 0, sizeof(fixture->device));
    memset(&fixture->unit, 0, sizeof(fixture->unit));
    memset(&fixture->line_units, 0, sizeof(fixture->line_units));
    memset(&fixture->out_units, 0, sizeof(fixture->out_units));
    memset(&fixture->mux, 0, sizeof(fixture->mux));
    memset(&fixture->lnorder, 0, sizeof(fixture->lnorder));
    memset(&fixture->lines, 0, sizeof(fixture->lines));
    memset(&fixture->io, 0, sizeof(fixture->io));
    fixture->io.master_sock_result = INVALID_SOCKET;
    fixture->io.master_sock_status = SCPE_OPENERR;
    fixture->io.next_accept_result = 4;
    for (i = 0; i < 4; i++)
        fixture->io.accept_results[i] = INVALID_SOCKET;
    fixture->io.connect_result = INVALID_SOCKET;
    fixture->io.next_check_conn_result = 4;
    fixture->io.open_serial_result = INVALID_HANDLE;
    fixture->io.open_serial_status = SCPE_OPENERR;
    fixture->io.eth_open_result = SCPE_OK;
    fixture->io.next_eth_read_result = 4;
    fixture->io.eth_write_result = SCPE_OK;
    fixture->io.eth_filter_result = SCPE_OK;
    snprintf(fixture->io.sockname, sizeof(fixture->io.sockname), "%s",
             "127.0.0.1:1000");
    snprintf(fixture->io.peername, sizeof(fixture->io.peername), "%s",
             "127.0.0.1:2000");

    fixture->device.name = "TMXR";
    assert_int_equal(simh_test_set_sim_name("zimh-unit-sim-tmxr"), 0);
    fixture->mux.dptr = &fixture->device;
    fixture->mux.ldsc = fixture->lines;
    fixture->mux.lines = 4;
    fixture->mux.uptr = &fixture->unit;
    fixture->mux.lnorder = fixture->lnorder;
    fixture->mux.ring_sock = INVALID_SOCKET;
    for (i = 0; i < 4; i++)
        fixture->lnorder[i] = -1;

    for (i = 0; i < fixture->mux.lines; i++) {
        fixture->lines[i].mp = &fixture->mux;
        fixture->lines[i].dptr = &fixture->device;
        fixture->lines[i].uptr = &fixture->line_units[i];
        fixture->lines[i].o_uptr = &fixture->out_units[i];
        fixture->lines[i].rxbsz = 16;
        fixture->lines[i].txbsz = 16;
        sim_expect_init_context(&fixture->lines[i].expect, &fixture->device,
                                TMXR_DBG_EXP);
        sim_send_init_context(&fixture->lines[i].send, &fixture->device,
                              TMXR_DBG_SEND);
    }

    tmxr_io_fixture = fixture;
    sim_init_sock();
    tmxr_reset_io_hooks();

    *state = fixture;
    return 0;
}

static int teardown_sim_tmxr_fixture(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    int32 i;

    for (i = 0; i < fixture->mux.lines; i++) {
        sim_send_clear(&fixture->lines[i].send);
        if (fixture->lines[i].txlogref != NULL)
            sim_close_logfile(&fixture->lines[i].txlogref);
        free(fixture->lines[i].ipad);
        free(fixture->lines[i].port);
        free(fixture->lines[i].acl);
        free(fixture->lines[i].txlogname);
        free(fixture->lines[i].rxb);
        free(fixture->lines[i].rbr);
        free(fixture->lines[i].txb);
        free(fixture->lines[i].rxpb);
        free(fixture->lines[i].txpb);
        free(fixture->lines[i].telnet_sent_opts);
        free(fixture->lines[i].serconfig);
        free(fixture->lines[i].destination);
        free(fixture->lines[i].lpb);
    }
    free(fixture->mux.port);
    free(fixture->mux.acl);
    free(fixture->mux.ring_ipad);
    free(fixture->unit.filename);
    tmxr_reset_io_hooks();
    sim_cleanup_sock();
    tmxr_io_fixture = NULL;
    free(fixture);
    *state = NULL;
    return 0;
}

/* Verify loopback toggling updates the line flag and allocates or frees
   the private loopback buffer. */
static void test_tmxr_loopback_toggle_updates_buffer_state(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];

    assert_false(tmxr_get_line_loopback(line));
    assert_null(line->lpb);

    assert_int_equal(tmxr_set_line_loopback(line, true), SCPE_OK);
    assert_true(tmxr_get_line_loopback(line));
    assert_non_null(line->lpb);
    assert_int_equal(line->lpbsz, line->rxbsz);
    assert_true(line->ser_connect_pending);

    assert_int_equal(tmxr_set_line_loopback(line, false), SCPE_OK);
    assert_false(tmxr_get_line_loopback(line));
    assert_null(line->lpb);
    assert_int_equal(line->lpbsz, 0);
}

/* Verify half-duplex mode toggles independently of other line state. */
static void test_tmxr_halfduplex_toggle_round_trips(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];

    assert_false(tmxr_get_line_halfduplex(line));
    assert_int_equal(tmxr_set_line_halfduplex(line, true), SCPE_OK);
    assert_true(tmxr_get_line_halfduplex(line));
    assert_int_equal(tmxr_set_line_halfduplex(line, false), SCPE_OK);
    assert_false(tmxr_get_line_halfduplex(line));
}

/* Verify receive and transmit queue helpers account for both linear and
   wrapped circular-buffer state. */
static void test_tmxr_queue_length_helpers_handle_wraparound(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];

    line->rxbpr = 2;
    line->rxbpi = 6;
    assert_int_equal(tmxr_rqln(line), 4);
    assert_int_equal(tmxr_input_pending_ln(line), 4);

    line->rxbpr = 10;
    line->rxbpi = 3;
    assert_int_equal(tmxr_rqln(line), 9);
    assert_int_equal(tmxr_input_pending_ln(line), -7);

    line->txbpr = 5;
    line->txbpi = 9;
    assert_int_equal(tmxr_tqln(line), 4);

    line->txbpr = 12;
    line->txbpi = 3;
    assert_int_equal(tmxr_tqln(line), 7);

    line->txppsize = 20;
    line->txppoffset = 5;
    assert_int_equal(tmxr_tpqln(line), 15);
    assert_true(tmxr_tpbusyln(line));

    line->txppoffset = line->txppsize;
    assert_int_equal(tmxr_tpqln(line), 0);
    assert_false(tmxr_tpbusyln(line));
}

/* Verify poll-interval validation rejects zero and stores accepted
   values on the multiplexer descriptor. */
static void test_tmxr_connection_poll_interval_validates_zero(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;

    assert_int_equal(tmxr_connection_poll_interval(&fixture->mux, 0), SCPE_ARG);
    assert_int_equal(tmxr_connection_poll_interval(&fixture->mux, 5), SCPE_OK);
    assert_int_equal(fixture->mux.poll_interval, 5);
}

/* Verify line-speed configuration sets per-line timing and attached unit
   wait values from one explicit speed. */
static void test_tmxr_set_line_speed_updates_timing_state(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];
    UNIT out_unit;

    memset(&out_unit, 0, sizeof(out_unit));
    line->uptr = &fixture->unit;
    line->o_uptr = &out_unit;

    assert_int_equal(tmxr_set_line_speed(line, "9600"), SCPE_OK);
    assert_int_equal(line->rxbps, 9600);
    assert_int_equal(line->txbps, 9600);
    assert_true(line->bpsfactor == 1.0);
    assert_int_equal(line->rxdeltausecs, TMLN_SPD_9600_BPS);
    assert_int_equal(line->txdeltausecs, TMLN_SPD_9600_BPS);
    assert_true(line->rxnexttime == 0.0);
    assert_int_equal(fixture->unit.wait, TMLN_SPD_9600_BPS);
    assert_int_equal(out_unit.wait, TMLN_SPD_9600_BPS);
}

/* Verify factor-only speed updates reuse the current receive speed when the
   line is not attached to a serial port. */
static void test_tmxr_set_line_speed_factor_only_reuses_current_bps(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[1];
    UNIT out_unit;

    memset(&out_unit, 0, sizeof(out_unit));
    line->uptr = &fixture->unit;
    line->o_uptr = &out_unit;
    line->rxbps = 9600;

    assert_int_equal(tmxr_set_line_speed(line, "*2"), SCPE_OK);
    assert_int_equal(line->rxbps, 9600);
    assert_true(line->bpsfactor == 2.0);
    assert_int_equal(line->rxdeltausecs, TMLN_SPD_9600_BPS / 2);
    assert_int_equal(line->txdeltausecs, TMLN_SPD_9600_BPS / 2);
    assert_int_equal(fixture->unit.wait, TMLN_SPD_9600_BPS / 2);
    assert_int_equal(out_unit.wait, TMLN_SPD_9600_BPS / 2);
}

/* Verify invalid line-speed strings are rejected cleanly. */
static void test_tmxr_set_line_speed_rejects_invalid_input(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];

    assert_int_equal(tmxr_set_line_speed(line, NULL), SCPE_2FARG);
    assert_int_equal(tmxr_set_line_speed(line, ""), SCPE_2FARG);
    assert_int_equal(tmxr_set_line_speed(line, "bogus"), SCPE_ARG);
    assert_int_equal(tmxr_set_line_speed(line, "9600*99"), SCPE_ARG);
}

/* Verify modem-control passthrough toggles on an unattached mux and rejects
   changes while a listener is active. */
static void test_tmxr_modem_control_passthru_toggles_and_rejects_attached_mux(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    int32 i;

    assert_int_equal(tmxr_set_modem_control_passthru(&fixture->mux), SCPE_OK);
    assert_true(fixture->mux.modem_control);
    for (i = 0; i < fixture->mux.lines; i++)
        assert_true(fixture->lines[i].modem_control);

    fixture->mux.master = (SOCKET)(uintptr_t)77;
    assert_int_equal(tmxr_clear_modem_control_passthru(&fixture->mux),
                     SCPE_ALATT);
    fixture->mux.master = 0;

    assert_int_equal(tmxr_clear_modem_control_passthru(&fixture->mux), SCPE_OK);
    assert_false(fixture->mux.modem_control);
    for (i = 0; i < fixture->mux.lines; i++)
        assert_false(fixture->lines[i].modem_control);
}

/* Verify per-line input/output unit assignment updates the target pointers and
   polling dynflags, while rejecting invalid line numbers. */
static void test_tmxr_set_line_units_update_poll_state(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    UNIT prior_in;
    UNIT prior_out;

    memset(&prior_in, 0, sizeof(prior_in));
    memset(&prior_out, 0, sizeof(prior_out));
    prior_in.dynflags = UNIT_TM_POLL;
    prior_out.dynflags = UNIT_TM_POLL;
    fixture->lines[0].uptr = &prior_in;
    fixture->lines[0].o_uptr = &prior_out;
    fixture->line_units[1].tmxr = &fixture->mux;
    fixture->out_units[1].tmxr = &fixture->mux;

    assert_int_equal(tmxr_set_line_unit(&fixture->mux, -1,
                                        &fixture->line_units[1]),
                     SCPE_ARG);
    assert_int_equal(tmxr_set_line_output_unit(&fixture->mux, 9,
                                               &fixture->out_units[1]),
                     SCPE_ARG);

    assert_int_equal(tmxr_set_line_unit(&fixture->mux, 0,
                                        &fixture->line_units[1]),
                     SCPE_OK);
    assert_ptr_equal(fixture->lines[0].uptr, &fixture->line_units[1]);
    assert_int_equal(prior_in.dynflags & UNIT_TM_POLL, 0);
    assert_true(fixture->line_units[1].dynflags & UNIT_TM_POLL);

    assert_int_equal(tmxr_set_line_output_unit(&fixture->mux, 0,
                                               &fixture->out_units[1]),
                     SCPE_OK);
    assert_ptr_equal(fixture->lines[0].o_uptr, &fixture->out_units[1]);
    assert_int_equal(prior_out.dynflags & UNIT_TM_POLL, 0);
    assert_true(fixture->out_units[1].dynflags & UNIT_TM_POLL);
}

/* Verify the per-line half-duplex and modem-control setters toggle the stored
   state cleanly. */
static void test_tmxr_line_mode_setters_toggle_state(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];

    assert_int_equal(tmxr_set_line_halfduplex(line, true), SCPE_OK);
    assert_true(tmxr_get_line_halfduplex(line));
    assert_int_equal(tmxr_set_line_halfduplex(line, false), SCPE_OK);
    assert_false(tmxr_get_line_halfduplex(line));

    assert_int_equal(tmxr_set_line_modem_control(line, true), SCPE_OK);
    assert_true(line->modem_control);
    assert_int_equal(tmxr_set_line_modem_control(line, false), SCPE_OK);
    assert_false(line->modem_control);
}

/* Verify non-serial line configuration stores the config text and updates
   the derived line-speed state. */
static void test_tmxr_set_config_line_updates_cached_config_and_speed(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];

    line->uptr = &fixture->unit;

    assert_int_equal(tmxr_set_config_line(line, "19200-8N1"), SCPE_OK);
    assert_non_null(line->serconfig);
    assert_string_equal(line->serconfig, "19200-8N1");
    assert_int_equal(line->rxbps, 19200);
    assert_int_equal(line->txbps, 19200);
    assert_int_equal(line->rxdeltausecs, TMLN_SPD_19200_BPS);
    assert_int_equal(line->txdeltausecs, TMLN_SPD_19200_BPS);
}

/* Verify invalid non-serial configuration is rejected and the cached config
   text is discarded. */
static void test_tmxr_set_config_line_rejects_invalid_config(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[1];

    assert_int_equal(tmxr_set_config_line(line, "bogus"), SCPE_ARG);
    assert_null(line->serconfig);
}

/* Verify mux-wide BUFFERED with no explicit size selects the documented
   default and resizes all line buffers. */
static void test_tmxr_open_master_buffered_defaults_to_32768(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    int32 i;

    assert_int_equal(tmxr_open_master(&fixture->mux, "BUFFERED"), SCPE_OK);
    assert_int_equal(fixture->mux.buffered, 32768);
    for (i = 0; i < fixture->mux.lines; i++) {
        assert_int_equal(fixture->lines[i].txbsz, 32768);
        assert_int_equal(fixture->lines[i].rxbsz, 32768);
        assert_true(fixture->lines[i].txbfd);
    }
    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify a mux listener specification validates the port, opens the listener,
   and applies mux-wide telnet/message flags. */
static void test_tmxr_open_master_listener_sets_mux_listener_state(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;

    install_tmxr_test_io_hooks();
    fixture->io.master_sock_result = (SOCKET)(uintptr_t)44;
    fixture->io.master_sock_status = SCPE_OK;

    assert_int_equal(
        tmxr_open_master(&fixture->mux,
                         "1234;notelnet;nomessage;accept=127.0.0.1;"
                         "reject=10.0.0.0/8"),
        SCPE_OK);
    assert_int_equal(fixture->io.master_sock_calls, 2);
    assert_string_equal(fixture->io.last_master_sock_port, "1234");
    assert_int_equal(fixture->io.close_sock_calls, 1);
    assert_int_equal((int)(uintptr_t)fixture->io.closed_socks[0], 44);
    assert_int_equal(fixture->io.sleep_calls, 1);
    assert_int_equal(fixture->io.sleep_msec[0], 2);
    assert_int_equal((int)(uintptr_t)fixture->mux.master, 44);
    assert_non_null(fixture->mux.port);
    assert_string_equal(fixture->mux.port, "1234");
    assert_true(fixture->mux.notelnet);
    assert_true(fixture->mux.nomessage);
    assert_non_null(fixture->mux.acl);
    assert_string_equal(fixture->mux.acl, "+127.0.0.1,-10.0.0.0/8");

    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
    assert_int_equal(fixture->io.close_sock_calls, 2);
}

/* Verify mux-wide BUFFERED accepts an explicit size and propagates it to all
   line buffers. */
static void test_tmxr_open_master_buffered_accepts_explicit_size(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    int32 i;

    assert_int_equal(tmxr_open_master(&fixture->mux, "BUFFERED=128"), SCPE_OK);
    assert_int_equal(fixture->mux.buffered, 128);
    for (i = 0; i < fixture->mux.lines; i++) {
        assert_int_equal(fixture->lines[i].txbsz, 128);
        assert_int_equal(fixture->lines[i].rxbsz, 128);
        assert_true(fixture->lines[i].txbfd);
    }
    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify LINE= with no value is rejected as a missing line specifier. */
static void test_tmxr_open_master_rejects_missing_line_specifier(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    status = tmxr_open_master(&fixture->mux, "Line=");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_2FARG);
}

/* Verify LINE= rejects out-of-range line numbers. */
static void test_tmxr_open_master_rejects_invalid_line_specifier(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    status = tmxr_open_master(&fixture->mux, "Line=9");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
}

/* Verify SHOW SYNC reports when synchronous Ethernet support is unavailable. */
static void test_tmxr_show_sync_reports_missing_network_support(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    char *output = NULL;
    size_t output_size = 0;
    FILE *stream;

    install_tmxr_test_io_hooks();
    fixture->io.eth_devices_result = -1;
    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);

    assert_int_equal(
        tmxr_show_sync_devices(stream, &fixture->device, &fixture->unit, 0,
                               NULL),
        SCPE_OK);
    assert_int_equal(fflush(stream), 0);
    assert_int_equal(fclose(stream), 0);
    assert_non_null(output);
    assert_non_null(strstr(output, "DDCMP synchronous link devices:\n"));
    assert_non_null(
        strstr(output, "network support not available in simulator\n"));
    free(output);
}

/* Verify a line-specific listener uses the listener hook and retains the
   line-local telnet/message flags. */
static void test_tmxr_open_master_line_listener_sets_line_state(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[2];

    install_tmxr_test_io_hooks();
    fixture->io.master_sock_result = (SOCKET)(uintptr_t)45;
    fixture->io.master_sock_status = SCPE_OK;

    assert_int_equal(
        tmxr_open_master(&fixture->mux,
                         "Line=2,2345;notelnet;nomessage;accept=127.0.0.1"),
        SCPE_OK);
    assert_int_equal(fixture->io.master_sock_calls, 2);
    assert_string_equal(fixture->io.last_master_sock_port, "2345");
    assert_int_equal(fixture->io.close_sock_calls, 1);
    assert_int_equal(fixture->io.sleep_calls, 1);
    assert_int_equal(fixture->io.sleep_msec[0], 2);
    assert_int_equal((int)(uintptr_t)line->master, 45);
    assert_non_null(line->port);
    assert_string_equal(line->port, "2345");
    assert_true(line->notelnet);
    assert_true(line->nomessage);
    assert_non_null(line->acl);
    assert_string_equal(line->acl, "+127.0.0.1");

    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
    assert_int_equal(fixture->io.close_sock_calls, 2);
}

/* Verify invalid listener ACL criteria are rejected before any port-open
   attempt is made. */
static void test_tmxr_open_master_rejects_invalid_listener_acl(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    install_tmxr_test_io_hooks();
    fixture->io.master_sock_result = (SOCKET)(uintptr_t)44;
    fixture->io.master_sock_status = SCPE_OK;

    status = tmxr_open_master(&fixture->mux, "1234;accept=bogus");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
    assert_int_equal(fixture->io.master_sock_calls, 1);
}

/* Verify invalid listener option keywords are rejected before any port-open
   attempt is made. */
static void test_tmxr_open_master_rejects_invalid_listener_option(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    install_tmxr_test_io_hooks();
    fixture->io.master_sock_result = (SOCKET)(uintptr_t)44;
    fixture->io.master_sock_status = SCPE_OK;

    status = tmxr_open_master(&fixture->mux, "1234;bogus");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
    assert_int_equal(fixture->io.master_sock_calls, 1);
}

/* Verify SYNC=syncN translates the host framer alias through the Ethernet
   inventory hook before opening the framer device. */
static void test_tmxr_open_master_translates_sync_aliases_via_eth_inventory(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[1];

    install_tmxr_test_io_hooks();
    fixture->io.eth_devices_result = 1;
    snprintf(fixture->io.eth_devices_list[0].name,
             sizeof(fixture->io.eth_devices_list[0].name), "%s", "framer0");
    fixture->io.eth_devices_list[0].eth_api = ETH_API_PCAP;

    assert_int_equal(
        tmxr_open_master(&fixture->mux, "Line=1,SYNC=sync0:integral:56000"),
        SCPE_OK);
    assert_int_equal(fixture->io.eth_devices_calls, 1);
    assert_int_equal(fixture->io.eth_open_calls, 1);
    assert_string_equal(fixture->io.last_eth_open_name, "framer0");
    assert_int_equal(fixture->io.eth_filter_calls, 1);
    assert_non_null(line->framer);
    assert_true(line->datagram);
    assert_true(line->notelnet);
    assert_int_equal(line->rxdeltausecs, (uint32)(8000000 / 56000));
    assert_int_equal(line->txdeltausecs, (uint32)(8000000 / 56000));

    assert_int_equal(tmxr_detach_ln(line), SCPE_OK);
    assert_int_equal(fixture->io.eth_close_calls, 1);
    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify malformed status frames from a synchronous framer are ignored rather
   than being copied as a negative-length status message. */
static void test_tmxr_poll_rx_ignores_short_framer_status_frame(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[1];

    install_tmxr_test_io_hooks();
    fixture->io.eth_devices_result = 1;
    snprintf(fixture->io.eth_devices_list[0].name,
             sizeof(fixture->io.eth_devices_list[0].name), "%s", "framer0");
    fixture->io.eth_devices_list[0].eth_api = ETH_API_PCAP;

    assert_int_equal(
        tmxr_open_master(&fixture->mux, "Line=1,SYNC=sync0:integral:56000"),
        SCPE_OK);

    set_test_framer_packet_header(&fixture->io.eth_read_packets[0], 1, 021);
    fixture->io.eth_read_results[0] = 1;
    fixture->io.eth_read_results[1] = 0;
    fixture->io.next_eth_read_result = 0;
    line->rcve = 1;

    tmxr_poll_rx(&fixture->mux);

    assert_int_equal(fixture->io.eth_read_calls, 2);
    assert_int_equal(tmxr_rqln(line), 0);

    assert_int_equal(tmxr_detach_ln(line), SCPE_OK);
    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify malformed data frames from a synchronous framer are discarded and do
   not prevent a later valid frame from being received. */
static void test_tmxr_poll_rx_skips_short_framer_data_frame(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[1];
    int32 value;

    install_tmxr_test_io_hooks();
    fixture->io.eth_devices_result = 1;
    snprintf(fixture->io.eth_devices_list[0].name,
             sizeof(fixture->io.eth_devices_list[0].name), "%s", "framer0");
    fixture->io.eth_devices_list[0].eth_api = ETH_API_PCAP;

    assert_int_equal(
        tmxr_open_master(&fixture->mux, "Line=1,SYNC=sync0:integral:56000"),
        SCPE_OK);

    set_test_framer_packet_header(&fixture->io.eth_read_packets[0], 1, 'X');
    set_test_framer_packet_header(&fixture->io.eth_read_packets[1], 3, 'A');
    fixture->io.eth_read_results[0] = 1;
    fixture->io.eth_read_results[1] = 1;
    fixture->io.next_eth_read_result = 0;
    line->rcve = 1;
    line->conn = true;
    line->rxbps = 0;

    tmxr_poll_rx(&fixture->mux);
    value = tmxr_getc_ln(line);

    assert_int_equal(fixture->io.eth_read_calls, 2);
    assert_true(value & TMXR_VALID);
    assert_int_equal(value & 0xFF, 'A');

    assert_int_equal(tmxr_detach_ln(line), SCPE_OK);
    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify framer write failures are reported as lost output instead of being
   mistaken for successful packet transmission. */
static void test_tmxr_put_packet_ln_reports_framer_write_failure(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[1];
    const uint8 payload[] = {0x11, 0x22, 0x33};

    install_tmxr_test_io_hooks();
    fixture->io.eth_devices_result = 1;
    snprintf(fixture->io.eth_devices_list[0].name,
             sizeof(fixture->io.eth_devices_list[0].name), "%s", "framer0");
    fixture->io.eth_devices_list[0].eth_api = ETH_API_PCAP;

    assert_int_equal(
        tmxr_open_master(&fixture->mux, "Line=1,SYNC=sync0:integral:56000"),
        SCPE_OK);
    line->conn = true;
    fixture->io.eth_write_calls = 0;
    fixture->io.eth_write_result = SCPE_IOERR;

    assert_int_equal(tmxr_put_packet_ln(line, payload, sizeof(payload)),
                     SCPE_LOST);
    assert_int_not_equal(fixture->io.eth_write_calls, 0);
    assert_int_equal(fixture->io.eth_write_packets[0].msg[14], 0x03);
    assert_int_equal(fixture->io.eth_write_packets[0].msg[15], 0x00);
    assert_int_equal(fixture->io.eth_write_packets[0].msg[16], 0x11);
    assert_int_equal(fixture->io.eth_write_packets[0].msg[17], 0x22);
    assert_int_equal(fixture->io.eth_write_packets[0].msg[18], 0x33);
    assert_null(line->framer);
    assert_false(line->conn);

    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify framer start treats a status received during the command write as the
   response to that command instead of waiting for a later status packet. */
static void test_tmxr_start_framer_uses_prewrite_status_snapshot(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[1];
    int32 status_bits = 0;

    install_tmxr_test_io_hooks();
    fixture->io.eth_devices_result = 1;
    snprintf(fixture->io.eth_devices_list[0].name,
             sizeof(fixture->io.eth_devices_list[0].name), "%s", "framer0");
    fixture->io.eth_devices_list[0].eth_api = ETH_API_PCAP;

    assert_int_equal(
        tmxr_open_master(&fixture->mux, "Line=1,SYNC=sync0:integral:56000"),
        SCPE_OK);
    line->conn = true;
    line->rcve = true;
    fixture->io.eth_write_calls = 0;
    fixture->io.eth_read_calls = 0;
    fixture->io.sleep_calls = 0;
    set_test_framer_status_packet(&fixture->io.eth_read_packets[0], 1);
    fixture->io.eth_read_results[0] = 1;
    fixture->io.next_eth_read_result = 0;
    fixture->io.poll_rx_after_eth_write = true;

    assert_int_equal(
        tmxr_set_get_modem_bits(line, TMXR_MDM_DTR, 0, &status_bits), SCPE_OK);

    assert_int_equal(fixture->io.eth_write_calls, 1);
    assert_int_equal(fixture->io.eth_read_calls, 2);
    assert_int_equal(fixture->io.sleep_calls, 0);
    assert_int_equal(status_bits, TMXR_MDM_DTR | TMXR_MDM_DSR);

    assert_int_equal(tmxr_detach_ln(line), SCPE_OK);
    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify failed framer start commands do not wait for a status response to a
   command that was never written. */
static void test_tmxr_start_framer_skips_status_wait_after_write_failure(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[1];
    int32 status_bits = 0;

    install_tmxr_test_io_hooks();
    fixture->io.eth_devices_result = 1;
    snprintf(fixture->io.eth_devices_list[0].name,
             sizeof(fixture->io.eth_devices_list[0].name), "%s", "framer0");
    fixture->io.eth_devices_list[0].eth_api = ETH_API_PCAP;

    assert_int_equal(
        tmxr_open_master(&fixture->mux, "Line=1,SYNC=sync0:integral:56000"),
        SCPE_OK);
    line->conn = true;
    fixture->io.eth_write_calls = 0;
    fixture->io.eth_read_calls = 0;
    fixture->io.sleep_calls = 0;
    fixture->io.eth_write_result = SCPE_IOERR;

    assert_int_equal(
        tmxr_set_get_modem_bits(line, TMXR_MDM_DTR, 0, &status_bits), SCPE_OK);

    assert_int_equal(fixture->io.eth_write_calls, 1);
    assert_int_equal(fixture->io.eth_read_calls, 0);
    assert_int_equal(fixture->io.sleep_calls, 0);

    assert_int_equal(tmxr_detach_ln(line), SCPE_OK);
    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify failed framer stop commands do not wait for a status response to a
   command that was never written. */
static void test_tmxr_stop_framer_skips_status_wait_after_write_failure(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[1];
    int32 status_bits = 0;

    install_tmxr_test_io_hooks();
    fixture->io.eth_devices_result = 1;
    snprintf(fixture->io.eth_devices_list[0].name,
             sizeof(fixture->io.eth_devices_list[0].name), "%s", "framer0");
    fixture->io.eth_devices_list[0].eth_api = ETH_API_PCAP;

    assert_int_equal(
        tmxr_open_master(&fixture->mux, "Line=1,SYNC=sync0:integral:56000"),
        SCPE_OK);
    line->conn = true;
    line->modembits = TMXR_MDM_DTR;
    fixture->io.eth_write_calls = 0;
    fixture->io.eth_read_calls = 0;
    fixture->io.sleep_calls = 0;
    fixture->io.eth_write_result = SCPE_IOERR;

    assert_int_equal(
        tmxr_set_get_modem_bits(line, 0, TMXR_MDM_DTR, &status_bits), SCPE_OK);

    assert_int_equal(fixture->io.eth_write_calls, 1);
    assert_int_equal(fixture->io.eth_read_calls, 0);
    assert_int_equal(fixture->io.sleep_calls, 0);

    assert_int_equal(tmxr_detach_ln(line), SCPE_OK);
    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify SHOW SYNC lists framer aliases using the host Ethernet inventory. */
static void test_tmxr_show_sync_lists_framer_aliases(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    char *output = NULL;
    size_t output_size = 0;
    FILE *stream;

    install_tmxr_test_io_hooks();
    fixture->io.eth_devices_result = 2;
    snprintf(fixture->io.eth_devices_list[0].name,
             sizeof(fixture->io.eth_devices_list[0].name), "%s", "framer0");
    snprintf(fixture->io.eth_devices_list[1].name,
             sizeof(fixture->io.eth_devices_list[1].name), "%s", "framer1");
    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);

    assert_int_equal(tmxr_show_sync(stream, &fixture->unit, 0, NULL), SCPE_OK);
    assert_int_equal(fflush(stream), 0);
    assert_int_equal(fclose(stream), 0);
    assert_non_null(output);
    assert_non_null(strstr(output, " sync0\tframer0\n"));
    assert_non_null(strstr(output, " sync1\tframer1\n"));
    free(output);
}

/* Verify loopback attach on a single-line mux updates loopback and line-speed
   state without needing host I/O. */
static void test_tmxr_open_master_loopback_sets_line_state(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];

    fixture->mux.lines = 1;

    assert_int_equal(
        tmxr_open_master(&fixture->mux, "Loopback,SPEED=2400"), SCPE_OK);
    assert_true(line->loopback);
    assert_int_equal(line->rxbps, 2400);
    assert_int_equal(line->txbps, 2400);
    assert_int_equal(line->rxdeltausecs, TMLN_SPD_2400_BPS);
    assert_int_equal(line->txdeltausecs, TMLN_SPD_2400_BPS);

    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify closing a loopback mux tears down the private loopback buffers. */
static void test_tmxr_close_master_clears_loopback_buffers(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];

    fixture->mux.lines = 1;

    assert_int_equal(tmxr_open_master(&fixture->mux, "Loopback"), SCPE_OK);
    assert_true(line->loopback);
    assert_non_null(line->lpb);
    assert_true(line->lpbsz > 0);

    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);

    assert_false(line->loopback);
    assert_null(line->lpb);
    assert_int_equal(line->lpbsz, 0);
}

/* Verify invalid BUFFERED sizes are rejected. */
static void test_tmxr_open_master_rejects_invalid_buffered_size(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    status = tmxr_open_master(&fixture->mux, "Buffered=0");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
}

/* Verify a line-specific DISABLED attach marks the line unavailable. */
static void test_tmxr_open_master_disabled_marks_line_unavailable(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[1];

    assert_int_equal(tmxr_open_master(&fixture->mux, "Line=1,Disabled"),
                     SCPE_OK);
    assert_int_equal(line->conn, TMXR_LINE_DISABLED);

    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify CONNECT= requires a non-empty destination. */
static void test_tmxr_open_master_rejects_missing_connect_specifier(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    status = tmxr_open_master(&fixture->mux, "Connect=");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_2FARG);
}

/* Verify SYNC= requires a non-empty framer specification. */
static void test_tmxr_open_master_rejects_missing_sync_specifier(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    status = tmxr_open_master(&fixture->mux, "Line=1,SYNC=");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_2FARG);
}

/* Verify SPEED= rejects unsupported speed syntax before any side effects. */
static void test_tmxr_open_master_rejects_invalid_speed_specifier(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    status = tmxr_open_master(&fixture->mux, "Speed=bogus");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
}

/* Verify port-speed-controlled muxes reject explicit line-speed settings
   during normal attach parsing. */
static void test_tmxr_open_master_rejects_programmatic_speed_override(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    fixture->mux.port_speed_control = true;
    status = tmxr_open_master(&fixture->mux, "Speed=9600");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
}

/* Verify the SIM_SW_REST restore path bypasses the port-speed-control reject
   guard and applies the requested line speed. */
static void test_tmxr_open_master_restores_speed_with_port_speed_control(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];
    int32 saved_switches = sim_switches;

    fixture->mux.lines = 1;
    fixture->mux.port_speed_control = true;
    sim_switches |= SIM_SW_REST;

    assert_int_equal(tmxr_open_master(&fixture->mux, "Loopback,Speed=2400"),
                     SCPE_OK);
    assert_int_equal(line->rxbps, 2400);
    assert_int_equal(line->txbps, 2400);

    sim_switches = saved_switches;
    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify single-line Connect=host:port uses the socket hook and caches the
   destination/address text on the line. */
static void test_tmxr_open_master_connect_sets_destination_and_ipad(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];

    fixture->mux.lines = 1;
    install_tmxr_test_io_hooks();
    fixture->io.connect_result = (SOCKET)(uintptr_t)88;

    assert_int_equal(
        tmxr_open_master(&fixture->mux, "Connect=remote:1234"), SCPE_OK);
    assert_int_equal(fixture->io.open_serial_calls, 2);
    assert_string_equal(fixture->io.last_open_serial_name, "remote:1234");
    assert_int_equal(fixture->io.connect_calls, 2);
    assert_string_equal(fixture->io.last_sourcehostport, "");
    assert_string_equal(fixture->io.last_hostport, "remote:1234");
    assert_int_equal((int)(uintptr_t)line->connecting, 88);
    assert_non_null(line->destination);
    assert_string_equal(line->destination, "remote:1234");
    assert_non_null(line->ipad);
    assert_string_equal(line->ipad, "remote:1234");

    assert_int_equal(tmxr_detach_ln(line), SCPE_OK);
    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify an ambiguous mux-wide destination is rejected before any host
   socket-connect validation is attempted. */
static void test_tmxr_open_master_rejects_ambiguous_destination(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    install_tmxr_test_io_hooks();
    fixture->io.connect_result = (SOCKET)(uintptr_t)99;

    status = tmxr_open_master(&fixture->mux, "Connect=remote:1234");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
    assert_int_equal(fixture->io.connect_calls, 0);
}

/* Verify a datagram destination without a local listen port is rejected
   before any host socket-connect validation is attempted. */
static void test_tmxr_open_master_rejects_datagram_connect_without_listen(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    install_tmxr_test_io_hooks();
    fixture->mux.lines = 1;
    fixture->io.connect_result = (SOCKET)(uintptr_t)98;

    status = tmxr_open_master(&fixture->mux, "Datagram,Connect=remote:1234");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
    assert_int_equal(fixture->io.connect_calls, 0);
}

/* Verify CONNECT=... and SYNC=... cannot be combined in the same attach
   specification. */
static void test_tmxr_open_master_rejects_connect_with_sync(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    install_tmxr_test_io_hooks();
    fixture->mux.lines = 1;

    status = tmxr_open_master(&fixture->mux,
                              "Connect=remote:1234,SYNC=en0:integral:56000");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
    assert_int_equal(fixture->io.open_serial_calls, 0);
    assert_int_equal(fixture->io.connect_calls, 0);
    assert_int_equal(fixture->io.eth_open_calls, 0);
}

/* Verify Datagram + Connect=... rejects a TELNET destination option before
   any host connect attempt is made. */
static void test_tmxr_open_master_rejects_datagram_telnet_destination(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    install_tmxr_test_io_hooks();
    fixture->mux.lines = 1;
    fixture->io.master_sock_status = SCPE_OK;
    fixture->io.master_sock_result = (SOCKET)(uintptr_t)77;

    status = tmxr_open_master(&fixture->mux,
                              "Datagram,1234,"
                              "Connect=remote:1234;TELNET");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
    assert_int_equal(fixture->io.connect_calls, 0);
}

/* Verify single-line Datagram + Connect=... uses the listen token as the
   local source endpoint and leaves the line in the connecting state. */
static void test_tmxr_open_master_datagram_connect_uses_listen_source(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];

    fixture->mux.lines = 1;
    install_tmxr_test_io_hooks();
    fixture->io.master_sock_status = SCPE_OK;
    fixture->io.master_sock_result = (SOCKET)(uintptr_t)77;
    fixture->io.connect_result = (SOCKET)(uintptr_t)88;

    assert_int_equal(tmxr_open_master(&fixture->mux,
                                      "Datagram,1234,Connect=remote:1234"),
                     SCPE_OK);
    assert_int_equal(fixture->io.connect_calls, 2);
    assert_string_equal(fixture->io.last_sourcehostport, "1234");
    assert_string_equal(fixture->io.last_hostport, "remote:1234");
    assert_int_equal(fixture->io.last_connect_opt_flags,
                     SIM_SOCK_OPT_DATAGRAM);
    assert_ptr_equal(line->connecting, (SOCKET)(uintptr_t)88);
    assert_true(line->datagram);
    assert_true(line->notelnet);
    assert_non_null(line->port);
    assert_string_equal(line->port, "1234");
    assert_non_null(line->destination);
    assert_string_equal(line->destination, "remote:1234");
    assert_non_null(line->ipad);
    assert_string_equal(line->ipad, "remote:1234");

    assert_int_equal(tmxr_detach_ln(line), SCPE_OK);
    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify unexpected destination-side options are rejected before host socket
   validation is attempted. */
static void test_tmxr_open_master_rejects_unexpected_connect_option(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    install_tmxr_test_io_hooks();
    fixture->mux.lines = 1;

    status = tmxr_open_master(&fixture->mux,
                              "Connect=remote:1234;BOGUS");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
    assert_int_equal(fixture->io.connect_calls, 0);
}

/* Verify DISABLED cannot be combined with an outgoing destination on the same
   line. */
static void test_tmxr_open_master_rejects_disabled_with_destination(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    install_tmxr_test_io_hooks();
    status = tmxr_open_master(&fixture->mux,
                              "Line=1,Disabled,Connect=remote:1234");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
    assert_int_equal(fixture->io.open_serial_calls, 0);
    assert_int_equal(fixture->io.connect_calls, 0);
}

/* Verify a single-line mux rejects a line-specific listener when a mux-wide
   listener is already active, after only a validation probe of the requested
   line-local port. */
static void test_tmxr_open_master_rejects_line_listener_when_mux_listener_exists(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    install_tmxr_test_io_hooks();
    fixture->mux.lines = 1;
    fixture->io.master_sock_status = SCPE_OK;
    fixture->io.master_sock_result = (SOCKET)(uintptr_t)111;

    assert_int_equal(tmxr_open_master(&fixture->mux, "1234"), SCPE_OK);
    assert_int_equal(fixture->io.master_sock_calls, 2);

    fixture->io.master_sock_result = (SOCKET)(uintptr_t)112;
    status = tmxr_open_master(&fixture->mux, "Line=0,2345");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
    assert_int_equal(fixture->io.master_sock_calls, 3);
    assert_int_equal(fixture->io.close_sock_calls, 2);

    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify framer configuration without an explicit line is rejected before
   any Ethernet device is opened. */
static void test_tmxr_open_master_rejects_framer_without_line(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    install_tmxr_test_io_hooks();
    fixture->io.eth_devices_result = 1;
    snprintf(fixture->io.eth_devices_list[0].name,
             sizeof(fixture->io.eth_devices_list[0].name), "%s", "en0");
    fixture->io.eth_devices_list[0].eth_api = ETH_API_PCAP;

    status = tmxr_open_master(&fixture->mux, "SYNC=sync0:integral:56000");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
    assert_int_equal(fixture->io.eth_open_calls, 0);
}

/* Verify a SYNC=syncN alias that does not exist in the host inventory fails
   before any Ethernet device is opened. */
static void test_tmxr_open_master_rejects_missing_sync_alias(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    install_tmxr_test_io_hooks();
    fixture->io.eth_devices_result = 0;

    status = tmxr_open_master(&fixture->mux, "Line=1,SYNC=sync0:integral:56000");
    assert_int_equal(status, SCPE_OPENERR);
    assert_int_equal(fixture->io.eth_devices_calls, 1);
    assert_int_equal(fixture->io.eth_open_calls, 0);
}

/* Verify a SYNC=syncN alias backed by a non-PCAP device is rejected before
   any Ethernet device is opened. */
static void test_tmxr_open_master_rejects_non_pcap_sync_alias(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    install_tmxr_test_io_hooks();
    fixture->io.eth_devices_result = 1;
    snprintf(fixture->io.eth_devices_list[0].name,
             sizeof(fixture->io.eth_devices_list[0].name), "%s", "raw0");
    fixture->io.eth_devices_list[0].eth_api = ETH_API_TAP;

    status = tmxr_open_master(&fixture->mux, "Line=1,SYNC=sync0:integral:56000");
    assert_int_equal(status, SCPE_OPENERR);
    assert_int_equal(fixture->io.eth_devices_calls, 1);
    assert_int_equal(fixture->io.eth_open_calls, 0);
}

/* Verify invalid framer modes are rejected before any Ethernet device is
   opened. */
static void test_tmxr_open_master_rejects_invalid_framer_mode(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    install_tmxr_test_io_hooks();

    status = tmxr_open_master(&fixture->mux, "Line=1,SYNC=en0:bogus:56000");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
    assert_int_equal(fixture->io.eth_open_calls, 0);
}

/* Verify integral framer mode rejects speeds below the supported minimum. */
static void test_tmxr_open_master_rejects_invalid_framer_speed(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    install_tmxr_test_io_hooks();

    status = tmxr_open_master(&fixture->mux,
                              "Line=1,SYNC=en0:integral:55999");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
    assert_int_equal(fixture->io.eth_open_calls, 0);
}

#if CBUFSIZE > (ETH_DEV_NAME_MAX + 32)
/* Verify overlong SYNC= device names are rejected before Ethernet open. */
static void test_tmxr_open_master_rejects_oversize_framer_device_name(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    const char prefix[] = "Line=1,SYNC=";
    const char suffix[] = ":integral:56000";
    char command[CBUFSIZE];
    char *cursor = command;
    t_stat status;

    install_tmxr_test_io_hooks();

    memcpy(cursor, prefix, sizeof(prefix) - 1);
    cursor += sizeof(prefix) - 1;
    memset(cursor, 'a', ETH_DEV_NAME_MAX);
    cursor += ETH_DEV_NAME_MAX;
    memcpy(cursor, suffix, sizeof(suffix));

    status = tmxr_open_master(&fixture->mux, command);
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
    assert_int_equal(fixture->io.eth_open_calls, 0);
}
#endif

/* Verify mux-wide DISABLED is rejected because disabling requires an explicit
   line selection. */
static void test_tmxr_open_master_rejects_disabled_without_line(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    status = tmxr_open_master(&fixture->mux, "DISABLED");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
}

/* Verify LOOPBACK on a multi-line mux is rejected as ambiguous. */
static void test_tmxr_open_master_rejects_ambiguous_loopback(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    t_stat status;

    status = tmxr_open_master(&fixture->mux, "LOOPBACK");
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
}

/* Verify verbose serial attach reports the pending connection through the
   TMXR-local sleep hook before leaving the line in the pending state. */
static void test_tmxr_open_master_verbose_serial_attach_uses_sleep_hook(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];
    int32 saved_switches = sim_switches;

    install_tmxr_test_io_hooks();
    fixture->io.open_serial_result = (SERHANDLE)(uintptr_t)5;
    fixture->io.open_serial_status = SCPE_OK;
    sim_switches |= SWMASK('V');

    assert_int_equal(tmxr_open_master(&fixture->mux, "Line=0,Connect=ser0"),
                     SCPE_OK);
    assert_true(fixture->io.open_serial_calls >= 1);
    assert_string_equal(fixture->io.last_open_serial_name, "ser0");
    assert_true(fixture->io.sleep_calls >= 1);
    assert_int_equal(fixture->io.sleep_msec[0], TMXR_DTR_DROP_TIME);
    assert_int_not_equal(fixture->io.write_serial_calls, 0);
    assert_ptr_equal(fixture->io.written_serial_ports[0],
                     (SERHANDLE)(uintptr_t)5);
    assert_ptr_equal(line->serport, (SERHANDLE)(uintptr_t)5);
    assert_true(line->ser_connect_pending);
    assert_false(line->conn);

    sim_switches = saved_switches;
    assert_int_equal(tmxr_detach_ln(line), SCPE_OK);
    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify the first outgoing-connection attempt in tmxr_poll_conn uses the
   TMXR-local connect hook rather than bypassing it. */
static void test_tmxr_poll_conn_initiates_outgoing_connection_via_hook(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];

    install_tmxr_test_io_hooks();
    fixture->io.connect_result = (SOCKET)(uintptr_t)66;
    line->destination = strdup("remote.example:4444");
    assert_non_null(line->destination);

    assert_int_equal(tmxr_poll_conn(&fixture->mux), -1);
    assert_int_equal(fixture->io.connect_calls, 1);
    assert_string_equal(fixture->io.last_sourcehostport, "");
    assert_string_equal(fixture->io.last_hostport, "remote.example:4444");
    assert_int_equal((int)(uintptr_t)line->connecting, 66);
}

/* Verify a line-specific listener accepts an incoming connection and retains
   the reported peer address on that line. */
static void test_tmxr_poll_conn_accepts_line_listener_connection(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[2];

    install_tmxr_test_io_hooks();
    line->master = (SOCKET)(uintptr_t)80;
    fixture->io.accept_results[0] = (SOCKET)(uintptr_t)81;
    fixture->io.accept_addresses[0] = "203.0.113.9:8000";
    fixture->io.next_accept_result = 0;

    assert_int_equal(tmxr_poll_conn(&fixture->mux), 2);
    assert_int_not_equal(line->conn, false);
    assert_int_equal((int)(uintptr_t)line->sock, 81);
    assert_non_null(line->ipad);
    assert_string_equal(line->ipad, "203.0.113.9:8000");
    assert_int_equal(line->sessions, 1);
    assert_int_not_equal(fixture->io.write_sock_calls, 0);
}

/* Verify an in-progress outgoing connection is finalized through the
   TMXR-local connection-state hooks. */
static void test_tmxr_poll_conn_marks_outgoing_connection_established(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];

    install_tmxr_test_io_hooks();
    fixture->io.check_conn_results[0] = 1;
    fixture->io.next_check_conn_result = 0;
    line->destination = strdup("remote.example:2222");
    assert_non_null(line->destination);
    line->connecting = (SOCKET)30;

    assert_int_equal(tmxr_poll_conn(&fixture->mux), 0);
    assert_true(line->conn);
    assert_int_equal((int)(uintptr_t)line->sock, 30);
    assert_int_equal((int)(uintptr_t)line->connecting, 0);
    assert_non_null(line->ipad);
    assert_string_equal(line->ipad, "remote.example:2222");
    assert_non_null(line->telnet_sent_opts);
    assert_int_equal(fixture->io.check_conn_calls, 1);
    assert_int_equal((int)(uintptr_t)fixture->io.checked_socks[0], 30);
    assert_int_equal(fixture->io.getnames_sock_calls, 1);
    assert_int_equal(fixture->io.write_sock_calls, 1);
}

/* Verify a failed outgoing connection is closed and immediately retried
   through the TMXR-local connect hooks. */
static void test_tmxr_poll_conn_retries_failed_outgoing_connection(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];

    install_tmxr_test_io_hooks();
    fixture->io.check_conn_results[0] = -1;
    fixture->io.next_check_conn_result = 0;
    fixture->io.connect_result = (SOCKET)44;
    line->destination = strdup("remote.example:3333");
    assert_non_null(line->destination);
    line->connecting = (SOCKET)30;

    assert_int_equal(tmxr_poll_conn(&fixture->mux), -1);
    assert_false(line->conn);
    assert_int_equal((int)(uintptr_t)line->connecting, 44);
    assert_int_equal(fixture->io.check_conn_calls, 1);
    assert_int_equal((int)(uintptr_t)fixture->io.checked_socks[0], 30);
    assert_int_equal(fixture->io.close_sock_calls, 1);
    assert_int_equal((int)(uintptr_t)fixture->io.closed_socks[0], 30);
    assert_int_equal(fixture->io.connect_calls, 1);
    assert_string_equal(fixture->io.last_hostport, "remote.example:3333");
}

/* Verify a mux-wide listener rejects a new session cleanly when every line is
   already busy. */
static void test_tmxr_poll_conn_reports_mux_listener_all_busy(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    int32 result;
    int32 i;

    install_tmxr_test_io_hooks();
    fixture->mux.master = (SOCKET)(uintptr_t)90;
    fixture->io.accept_results[0] = (SOCKET)(uintptr_t)91;
    fixture->io.accept_addresses[0] = "198.51.100.10:9000";
    fixture->io.next_accept_result = 0;

    for (i = 0; i < fixture->mux.lines; ++i)
        fixture->lines[i].conn = true;

    result = tmxr_poll_conn(&fixture->mux);
    assert_int_equal(result, -1);
    assert_int_equal(fixture->io.accept_calls, 1);
    assert_int_equal(fixture->io.write_sock_calls, 1);
    assert_int_equal((int)(uintptr_t)fixture->io.written_socks[0], 91);
    assert_string_equal(fixture->io.write_messages[0],
                        "All connections busy\r\n");
    assert_int_equal(fixture->io.close_sock_calls, 1);
    assert_int_equal((int)(uintptr_t)fixture->io.closed_socks[0], 91);
}

/* Verify a mux-wide listener parks a session in the ringing state when a
   modem-control line is otherwise available but DTR is still low. */
static void test_tmxr_poll_conn_rings_mux_listener_line_with_dtr_low(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *ringing_line = &fixture->lines[1];
    int32 result;
    int32 i;

    install_tmxr_test_io_hooks();
    fixture->mux.master = (SOCKET)(uintptr_t)92;
    fixture->io.accept_results[0] = (SOCKET)(uintptr_t)93;
    fixture->io.accept_addresses[0] = "198.51.100.11:9001";
    fixture->io.next_accept_result = 0;

    for (i = 0; i < fixture->mux.lines; ++i)
        fixture->lines[i].conn = true;

    ringing_line->conn = false;
    ringing_line->modem_control = true;
    ringing_line->modembits = 0;

    result = tmxr_poll_conn(&fixture->mux);
    assert_int_equal(result, -2);
    assert_true((ringing_line->modembits & TMXR_MDM_RNG) != 0);
    assert_int_equal((int)(uintptr_t)fixture->mux.ring_sock, 93);
    assert_non_null(fixture->mux.ring_ipad);
    assert_string_equal(fixture->mux.ring_ipad, "198.51.100.11:9001");
    assert_true(fixture->mux.ring_start_time != 0);
    assert_int_equal(fixture->io.close_sock_calls, 0);
}

/* Verify a ringing mux-wide listener eventually times out, clears ring
   indication, and tells the caller there was no answer. */
static void test_tmxr_poll_conn_times_out_ringing_mux_listener(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *ringing_line = &fixture->lines[0];
    int32 result;
    int32 i;

    install_tmxr_test_io_hooks();
    fixture->mux.master = (SOCKET)(uintptr_t)97;
    fixture->mux.ring_sock = (SOCKET)(uintptr_t)94;
    fixture->mux.ring_ipad = strdup("198.51.100.12:9002");
    fixture->mux.ring_start_time = 1;

    assert_non_null(fixture->mux.ring_ipad);

    for (i = 0; i < fixture->mux.lines; ++i)
        fixture->lines[i].conn = true;

    ringing_line->conn = false;
    ringing_line->modem_control = true;
    ringing_line->modembits = TMXR_MDM_RNG;

    result = tmxr_poll_conn(&fixture->mux);
    assert_int_equal(result, -2);
    assert_false(ringing_line->modembits & TMXR_MDM_RNG);
    assert_ptr_equal(fixture->mux.ring_sock, INVALID_SOCKET);
    assert_int_equal(fixture->mux.ring_start_time, 0);
    assert_int_equal(fixture->io.write_sock_calls, 1);
    assert_string_equal(fixture->io.write_messages[0],
                        "No answer on any connection\r\n");
    assert_int_equal(fixture->io.close_sock_calls, 1);
    assert_int_equal((int)(uintptr_t)fixture->io.closed_socks[0], 94);
}

/* Verify a line-specific listener rejects a connection whose source address
   does not match the configured null-modem destination. */
static void test_tmxr_poll_conn_rejects_line_listener_unexpected_source(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[2];

    install_tmxr_test_io_hooks();
    line->master = (SOCKET)(uintptr_t)95;
    line->destination = strdup("203.0.113.99:4000");
    fixture->io.accept_results[0] = (SOCKET)(uintptr_t)96;
    fixture->io.accept_addresses[0] = "198.51.100.13:9003";
    fixture->io.next_accept_result = 0;

    assert_non_null(line->destination);

    assert_int_equal(tmxr_poll_conn(&fixture->mux), -1);
    assert_false(line->conn);
    assert_ptr_equal(line->sock, 0);
    assert_null(line->ipad);
    assert_int_equal(fixture->io.accept_calls, 2);
    assert_int_equal(fixture->io.write_sock_calls, 1);
    assert_string_equal(
        fixture->io.write_messages[0],
        "Rejecting connection from unexpected source\r\n");
    assert_int_equal(fixture->io.close_sock_calls, 1);
    assert_int_equal((int)(uintptr_t)fixture->io.closed_socks[0], 96);
}

/* Verify closing a serial line closes the host port and clears the cached
   destination/configuration state. */
static void test_tmxr_close_ln_closes_serial_and_clears_cached_state(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];

    install_tmxr_test_io_hooks();
    line->serport = (SERHANDLE)(uintptr_t)1;
    line->destination = strdup("ser0");
    line->serconfig = strdup("9600-8N1");
    line->cnms = 7;
    line->xmte = 0;

    assert_non_null(line->destination);
    assert_non_null(line->serconfig);
    assert_int_equal(tmxr_close_ln(line), SCPE_OK);
    assert_int_equal(fixture->io.close_serial_calls, 1);
    assert_ptr_equal(fixture->io.closed_serial_ports[0],
                     (SERHANDLE)(uintptr_t)1);
    assert_ptr_equal(line->serport, 0);
    assert_null(line->destination);
    assert_null(line->serconfig);
    assert_int_equal(line->cnms, 0);
    assert_int_equal(line->xmte, 1);
}

/* Verify resetting a non-modem-control serial line pulses DTR/RTS instead of
   closing the underlying host port. */
static void test_tmxr_reset_ln_pulses_serial_control_lines(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[1];

    install_tmxr_test_io_hooks();
    line->serport = (SERHANDLE)(uintptr_t)2;
    line->modem_control = false;

    assert_int_equal(tmxr_reset_ln(line), SCPE_OK);
    assert_int_equal(fixture->io.close_serial_calls, 0);
    assert_int_equal(fixture->io.control_serial_calls, 3);
    assert_ptr_equal(fixture->io.control_serial_ports[0],
                     (SERHANDLE)(uintptr_t)2);
    assert_int_equal(fixture->io.control_bits_set[0], 0);
    assert_int_equal(fixture->io.control_bits_clear[0],
                     TMXR_MDM_DTR | TMXR_MDM_RTS);
    assert_ptr_equal(fixture->io.control_serial_ports[1],
                     (SERHANDLE)(uintptr_t)2);
    assert_int_equal(fixture->io.control_bits_set[1],
                     TMXR_MDM_DTR | TMXR_MDM_RTS);
    assert_int_equal(fixture->io.control_bits_clear[1], 0);
    assert_ptr_equal(fixture->io.control_serial_ports[2],
                     (SERHANDLE)(uintptr_t)2);
    assert_int_equal(fixture->io.control_bits_set[2], 0);
    assert_int_equal(fixture->io.control_bits_clear[2], 0);
    assert_int_equal(fixture->io.sleep_calls, 1);
    assert_int_equal(fixture->io.sleep_msec[0], TMXR_DTR_DROP_TIME);
}

/* Verify resetting a network line closes the current sockets and restarts any
   configured outgoing destination through the TMXR connect hook. */
static void test_tmxr_reset_ln_restarts_configured_outgoing_destination(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[2];

    install_tmxr_test_io_hooks();
    fixture->io.connect_result = (SOCKET)(uintptr_t)77;
    line->sock = (SOCKET)(uintptr_t)21;
    line->connecting = (SOCKET)(uintptr_t)22;
    line->conn = true;
    line->destination = strdup("remote:1234");
    line->ipad = strdup("127.0.0.1");

    assert_non_null(line->destination);
    assert_non_null(line->ipad);
    assert_int_equal(tmxr_reset_ln(line), SCPE_OK);
    assert_int_equal(fixture->io.close_sock_calls, 2);
    assert_int_equal((int)(uintptr_t)fixture->io.closed_socks[0], 21);
    assert_int_equal((int)(uintptr_t)fixture->io.closed_socks[1], 22);
    assert_int_equal(fixture->io.connect_calls, 1);
    assert_string_equal(fixture->io.last_sourcehostport, "");
    assert_string_equal(fixture->io.last_hostport, "remote:1234");
    assert_int_equal((int)(uintptr_t)line->connecting, 77);
    assert_ptr_equal(line->sock, 0);
    assert_null(line->ipad);
}

/* Verify raising DTR on a modem-control null-modem line starts the outgoing
   connection and reports the updated modem bits. */
static void test_tmxr_set_get_modem_bits_raises_dtr_and_starts_connect(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[3];
    int32 status_bits = 0;

    install_tmxr_test_io_hooks();
    fixture->io.connect_result = (SOCKET)(uintptr_t)55;
    line->modem_control = true;
    line->destination = strdup("remote:4321");
    line->port = strdup("listen:9999");

    assert_non_null(line->destination);
    assert_non_null(line->port);
    assert_int_equal(
        tmxr_set_get_modem_bits(line, TMXR_MDM_DTR, 0, &status_bits), SCPE_OK);
    assert_int_equal(fixture->io.connect_calls, 1);
    assert_string_equal(fixture->io.last_sourcehostport, "");
    assert_string_equal(fixture->io.last_hostport, "remote:4321");
    assert_int_equal((int)(uintptr_t)line->connecting, 55);
    assert_int_equal(status_bits, TMXR_MDM_DTR | TMXR_MDM_DSR);
}

/* Verify detaching a mux clears the unit attachment state and line polling
   flags without requiring live sockets or serial ports. */
static void test_tmxr_detach_clears_attached_state_and_line_poll_flags(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    int32 i;

    fixture->unit.flags = UNIT_ATT;
    fixture->unit.filename = strdup("attached");
    fixture->unit.tmxr = &fixture->mux;
    assert_non_null(fixture->unit.filename);

    for (i = 0; i < fixture->mux.lines; i++) {
        fixture->line_units[i].dynflags = UNIT_TM_POLL;
        fixture->line_units[i].tmxr = &fixture->mux;
        fixture->out_units[i].dynflags = UNIT_TM_POLL;
        fixture->out_units[i].tmxr = &fixture->mux;
    }

    assert_int_equal(tmxr_detach(&fixture->mux, &fixture->unit), SCPE_OK);
    assert_int_equal(fixture->unit.flags & UNIT_ATT, 0);
    assert_ptr_equal(fixture->unit.filename, NULL);
    assert_ptr_equal(fixture->unit.tmxr, NULL);
    for (i = 0; i < fixture->mux.lines; i++) {
        assert_int_equal(fixture->line_units[i].dynflags & UNIT_TM_POLL, 0);
        assert_ptr_equal(fixture->line_units[i].tmxr, NULL);
        assert_int_equal(fixture->out_units[i].dynflags & UNIT_TM_POLL, 0);
        assert_ptr_equal(fixture->out_units[i].tmxr, NULL);
    }
}

/* Verify the extracted connection banner helper suppresses output when line
   messages are disabled unless forced. */
static void test_tmxr_connection_message_honors_suppression_flags(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];

    line->notelnet = true;
    assert_null(tmxr_connection_message(&fixture->mux, line, false));

    line->notelnet = false;
    line->nomessage = true;
    assert_null(tmxr_connection_message(&fixture->mux, line, false));
}

/* Verify the extracted connection banner helper formats the simulator,
   device, and line fields for multi-line muxes. */
static void test_tmxr_connection_message_formats_multiline_banner(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[2];
    char *message;

    message = tmxr_connection_message(&fixture->mux, line, false);
    assert_non_null(message);
    assert_string_equal(
        message,
        "\n\r\nConnected to the zimh-unit-sim-tmxr simulator TMXR device,"
        " line 2\r\n\n");
    free(message);
}

/* Verify the extracted connection banner helper omits the line suffix for
   single-line muxes and can be forced despite message suppression. */
static void test_tmxr_connection_message_formats_single_line_forced_banner(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];
    char *message;

    fixture->mux.lines = 1;
    line->notelnet = true;

    message = tmxr_connection_message(&fixture->mux, line, true);
    assert_non_null(message);
    assert_string_equal(
        message,
        "\n\r\nConnected to the zimh-unit-sim-tmxr simulator TMXR device\r\n\n");
    free(message);
}

/* Verify an unconfigured line reports no attach-string state. */
static void test_tmxr_line_attach_string_returns_null_for_unconfigured_line(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;

    assert_null(tmxr_line_attach_string(&fixture->lines[0]));
}

/* Verify listener-style line configuration is rendered with the expected
   line prefix, option overrides, ACL entries, log file, and loopback state. */
static void test_tmxr_line_attach_string_formats_listener_state(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[1];
    char *attach;

    fixture->mux.buffered = 64;
    line->conn = TMXR_LINE_DISABLED;
    line->modem_control = true;
    line->txbfd = true;
    line->txbsz = 128;
    line->notelnet = true;
    line->nomessage = true;
    line->loopback = true;
    line->port = strdup("5000");
    line->acl = strdup("+127.0.0.1/32,-10.0.0.0/8");
    line->txlogname = strdup("tmxr.log");

    assert_non_null(line->port);
    assert_non_null(line->acl);
    assert_non_null(line->txlogname);

    attach = tmxr_line_attach_string(line);
    assert_non_null(attach);
    assert_string_equal(
        attach,
        "Line=1,Disabled,Modem,Buffered=128,5000;notelnet;nomessage;"
        "Accept=127.0.0.1/32;Reject=10.0.0.0/8,Log=tmxr.log,Loopback");
    free(attach);
}

/* Verify outgoing serial configuration is rendered using the serial port
   name and non-default line settings. */
static void test_tmxr_line_attach_string_formats_serial_destination(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];
    char *attach;

    fixture->mux.lines = 1;
    line->destination = strdup("ttyUSB0;ignored");
    line->serconfig = strdup("19200-7E1");
    line->serport = (SERHANDLE)(uintptr_t)1;

    assert_non_null(line->destination);
    assert_non_null(line->serconfig);

    attach = tmxr_line_attach_string(line);
    assert_non_null(attach);
    assert_string_equal(attach, ",Connect=ttyUSB0;19200-7E1");
    free(attach);
}

/* Verify unbuffered output on a disconnected line is rejected and counted as
   dropped output. */
static void test_tmxr_putc_ln_reports_lost_output_on_disconnected_line(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[1];

    line->conn = false;
    line->txbfd = false;
    line->notelnet = false;

    assert_int_equal(tmxr_putc_ln(line, 'A'), SCPE_LOST);
    assert_int_equal(line->txdrp, 1);
}

/* Verify Telnet output duplicates IAC bytes in the transmit buffer. */
static void test_tmxr_putc_ln_duplicates_telnet_iac(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[2];
    bool saved_running = sim_is_running;
    const unsigned char telnet_iac = 255;

    ensure_line_tx_buffer(line, 16);
    line->conn = true;
    line->notelnet = false;
    line->xmte = 1;
    sim_is_running = true;

    assert_int_equal(tmxr_putc_ln(line, telnet_iac), SCPE_OK);
    assert_int_equal(tmxr_tqln(line), 2);
    assert_int_equal((unsigned char)line->txb[0], telnet_iac);
    assert_int_equal((unsigned char)line->txb[1], telnet_iac);

    sim_is_running = saved_running;
}

/* Verify loopback output is sent through the buffered transmit path and can
   be read back as input. */
static void test_tmxr_putc_ln_loopback_round_trips_through_buffered_send(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line;
    int32 value;

    fixture->mux.lines = 1;
    line = &fixture->lines[0];

    assert_int_equal(
        tmxr_open_master(&fixture->mux, "BUFFERED=16,LOOPBACK"), SCPE_OK);
    assert_true(line->loopback);
    assert_true(line->txbfd);
    assert_int_equal(line->txbsz, 16);

    line->rcve = 1;
    line->xmte = 1;

    assert_int_equal(tmxr_putc_ln(line, 'A'), SCPE_OK);
    tmxr_poll_rx(&fixture->mux);
    value = tmxr_getc_ln(line);
    assert_true(value & TMXR_VALID);
    assert_int_equal(value & 0xFF, 'A');

    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify packet send reports transmit busy when a prior packet is still
   pending on the line. */
/* Verify raw/notelnet receive polling uses the full configured receive
   buffer instead of reserving Telnet guard space. */
static void test_tmxr_poll_rx_notelnet_uses_full_small_buffer(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line;
    int32 value;

    fixture->mux.lines = 1;
    line = &fixture->lines[0];

    assert_int_equal(
        tmxr_open_master(&fixture->mux, "BUFFERED=16,LOOPBACK"), SCPE_OK);
    assert_true(line->loopback);
    assert_true(line->txbfd);

    line->notelnet = true;
    line->rcve = 1;
    line->xmte = 1;
    line->rxbsz = 1;

    assert_int_equal(tmxr_putc_ln(line, 'Z'), SCPE_OK);
    tmxr_poll_rx(&fixture->mux);
    value = tmxr_getc_ln(line);
    assert_true(value & TMXR_VALID);
    assert_int_equal(value & 0xFF, 'Z');

    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify packet send reports transmit busy when a prior packet is still
   pending on the line. */
static void test_tmxr_put_packet_ln_stalls_when_packet_transmit_is_busy(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];
    const uint8 payload[] = {0x11, 0x22, 0x33};

    line->loopback = true;
    line->txppsize = 4;
    line->txppoffset = 0;
    line->txpb = calloc((size_t)line->txppsize, sizeof(*line->txpb));
    assert_non_null(line->txpb);

    assert_int_equal(tmxr_put_packet_ln(line, payload, sizeof(payload)),
                     SCPE_STALL);
    assert_true(tmxr_tpbusyln(line));
    assert_int_equal(line->txpcnt, 0);
}

/* Verify framed packet decoding directly from the receive buffer. */
static void test_tmxr_get_packet_ln_ex_decodes_framed_buffer(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line;
    const uint8 payload[] = {0x11, 0x22, 0x33};
    const uint8 packet[] = {0x7e, 0x00, 0x03, 0x11, 0x22, 0x33};
    const uint8 *received = NULL;
    size_t received_size = 0;

    fixture->mux.lines = 1;
    line = &fixture->lines[0];

    assert_int_equal(
        tmxr_open_master(&fixture->mux, "BUFFERED=16,LOOPBACK"), SCPE_OK);
    assert_true(line->txbfd);
    line->rcve = 1;
    line->notelnet = true;
    line->rxbps = 0;
    memcpy(line->rxb, packet, sizeof(packet));
    line->rxbpi = sizeof(packet);
    line->rxbpr = 0;

    assert_int_equal(
        tmxr_get_packet_ln_ex(line, &received, &received_size, 0x7e),
        SCPE_OK);
    assert_non_null(received);
    assert_int_equal(received_size, sizeof(payload));
    assert_memory_equal(received, payload, sizeof(payload));

    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

static void test_tmxr_getc_ln_reads_manual_buffered_byte(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line;
    int32 value;

    fixture->mux.lines = 1;
    line = &fixture->lines[0];

    assert_int_equal(
        tmxr_open_master(&fixture->mux, "BUFFERED=16,LOOPBACK"), SCPE_OK);
    assert_true(line->txbfd);
    line->rcve = 1;
    line->notelnet = true;
    line->rxbps = 0;
    line->rxb[0] = 0x7e;
    line->rxbpi = 1;
    line->rxbpr = 0;

    value = tmxr_getc_ln(line);
    assert_true(value & TMXR_VALID);
    assert_int_equal(value & 0xFF, 0x7e);

    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify packet loopback works on an unconnected notelnet loopback line
   without requiring conn to be forced true. */
static void test_tmxr_put_packet_ln_loopback_without_conn_round_trips(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line;
    const uint8 payload[] = {0x11, 0x22, 0x33};
    const uint8 *received = NULL;
    size_t received_size = 0;

    fixture->mux.lines = 1;
    line = &fixture->lines[0];

    assert_int_equal(
        tmxr_open_master(&fixture->mux, "BUFFERED=16,LOOPBACK"), SCPE_OK);
    assert_true(line->txbfd);
    line->notelnet = true;
    line->rcve = 1;
    line->rxbps = 0;
    line->txbps = 0;

    assert_int_equal(tmxr_put_packet_ln_ex(line, payload, sizeof(payload),
                                           0x7e),
                     SCPE_OK);
    tmxr_poll_rx(&fixture->mux);
    assert_int_equal(
        tmxr_get_packet_ln_ex(line, &received, &received_size, 0x7e),
        SCPE_OK);
    assert_int_equal(received_size, sizeof(payload));
    assert_non_null(received);
    assert_memory_equal(received, payload, sizeof(payload));

    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify empty packet receive distinguishes between connected and
   disconnected lines. */
static void test_tmxr_get_packet_ln_reports_empty_and_lost_states(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];
    const uint8 *received = NULL;
    size_t received_size = 0;

    line->conn = true;
    line->rcve = 1;

    assert_int_equal(
        tmxr_get_packet_ln_ex(line, &received, &received_size, 0x7e),
        SCPE_OK);
    assert_null(received);
    assert_int_equal(received_size, 0);

    line->conn = false;
    assert_int_equal(
        tmxr_get_packet_ln_ex(line, &received, &received_size, 0x7e),
        SCPE_LOST);
    assert_null(received);
    assert_int_equal(received_size, 0);
}

/* Verify poll_tx drains a queued loopback character and re-enables output
   when the transmit queue becomes empty. */
static void test_tmxr_poll_tx_drains_loopback_queue_and_reenables_xmte(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line;
    int32 value;

    fixture->mux.lines = 1;
    line = &fixture->lines[0];

    assert_int_equal(
        tmxr_open_master(&fixture->mux, "BUFFERED=16,LOOPBACK"), SCPE_OK);
    line->rcve = 1;
    line->txb[0] = 'Z';
    line->txbpi = 1;
    line->txbpr = 0;
    line->xmte = 0;

    tmxr_poll_tx(&fixture->mux);
    assert_int_equal(tmxr_tqln(line), 0);
    assert_true(line->xmte);

    tmxr_poll_rx(&fixture->mux);
    value = tmxr_getc_ln(line);
    assert_true(value & TMXR_VALID);
    assert_int_equal(value & 0xFF, 'Z');

    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify txdone transitions from pending to done and then to already-done
   once transmit timing has elapsed. */
static void test_tmxr_txdone_ln_tracks_pending_and_completed_output(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];

    line->conn = true;
    line->txbps = 9600;
    line->txdone = false;
    line->txnexttime = sim_gtime() + 1000.0;

    assert_int_equal(tmxr_txdone_ln(line), 0);

    line->txnexttime = sim_gtime() - 1.0;
    assert_int_equal(tmxr_txdone_ln(line), 1);
    assert_true(line->txdone);
    assert_int_equal(tmxr_txdone_ln(line), -1);
}

/* Verify line-order parsing preserves explicit order and fills in the
   remaining lines when ALL terminates the list. */
static void test_tmxr_set_lnorder_fills_unspecified_lines_with_all(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    char *output = NULL;
    size_t output_size = 0;
    FILE *stream;

    assert_int_equal(
        tmxr_set_lnorder(NULL, 0, "1;3;ALL", &fixture->mux), SCPE_OK);

    assert_int_equal(fixture->mux.lnorder[0], 1);
    assert_int_equal(fixture->mux.lnorder[1], 3);
    assert_int_equal(fixture->mux.lnorder[2], 0);
    assert_int_equal(fixture->mux.lnorder[3], 2);

    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);
    assert_int_equal(tmxr_show_lnorder(stream, NULL, 0, &fixture->mux), SCPE_OK);
    assert_int_equal(fclose(stream), 0);

    assert_non_null(output);
    assert_string_equal(output, "Order=1;3;0;2\n");
    free(output);
}

/* Verify malformed line-order input is rejected without disturbing the
   existing sequential-order marker. */
static void test_tmxr_set_lnorder_rejects_trailing_items_after_all(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;

    fixture->mux.lnorder[0] = -1;
    assert_int_equal(
        tmxr_set_lnorder(NULL, 0, "ALL;1", &fixture->mux),
        SCPE_2MARG | SCPE_NOMESSAGE);
    assert_int_equal(fixture->mux.lnorder[0], -1);
}

/* Verify line messages buffer bytes verbatim when no newline expansion is
   needed. */
static void test_tmxr_linemsg_buffers_plain_text(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];
    char *buffered;

    ensure_line_tx_buffer(line, 64);
    line->conn = true;
    line->txbfd = true;
    line->notelnet = true;

    tmxr_linemsg(line, "hello");

    buffered = copy_line_tx_buffer(line);
    assert_string_equal(buffered, "hello");
    free(buffered);
}

/* Verify formatted line messages expand bare LF into CRLF while preserving
   existing CRLF pairs. */
static void test_tmxr_linemsgf_expands_newlines_to_crlf(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];
    char *buffered;

    ensure_line_tx_buffer(line, 128);
    line->conn = true;
    line->txbfd = true;
    line->notelnet = true;

    tmxr_linemsgf(line, "one\ntwo\r\nthree");

    buffered = copy_line_tx_buffer(line);
    assert_string_equal(buffered, "one\r\ntwo\r\nthree");
    free(buffered);
}

/* Verify the varargs formatter grows beyond its stack buffer when needed and
   still emits the full formatted message. */
static void test_tmxr_linemsgf_handles_long_formatted_output(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];
    char long_text[1024];
    char *buffered;

    memset(long_text, 'A', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = '\0';

    ensure_line_tx_buffer(line, 2048);
    line->conn = true;
    line->txbfd = true;
    line->notelnet = true;

    tmxr_linemsgf(line, "<%s>", long_text);

    buffered = copy_line_tx_buffer(line);
    assert_int_equal((int)strlen(buffered), (int)strlen(long_text) + 2);
    assert_true(buffered[0] == '<');
    assert_true(buffered[strlen(buffered) - 1] == '>');
    free(buffered);
}

/* Verify the summary formatter distinguishes between single-line and
   multi-line muxes. */
static void test_tmxr_show_summ_formats_single_and_multi_line_state(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    char *output = NULL;
    size_t output_size = 0;
    FILE *stream;

    fixture->mux.lines = 1;
    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);
    assert_int_equal(tmxr_show_summ(stream, NULL, 0, &fixture->mux), SCPE_OK);
    assert_int_equal(fclose(stream), 0);
    assert_string_equal(output, "disconnected");
    free(output);

    output = NULL;
    output_size = 0;
    fixture->mux.lines = 4;
    fixture->lines[1].sock = (SOCKET)1;
    fixture->lines[2].serport = (SERHANDLE)(uintptr_t)1;
    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);
    assert_int_equal(tmxr_show_summ(stream, NULL, 0, &fixture->mux), SCPE_OK);
    assert_int_equal(fclose(stream), 0);
    assert_string_equal(output, "2 current connections");
    free(output);
}

/* Verify show-cstat reports disconnected states and connected line stats. */
static void test_tmxr_show_cstat_formats_disconnected_and_connected_states(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    char *output = NULL;
    size_t output_size = 0;
    FILE *stream;

    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);
    assert_int_equal(tmxr_show_cstat(stream, NULL, 0, &fixture->mux), SCPE_OK);
    assert_int_equal(fclose(stream), 0);
    assert_string_equal(output, "all disconnected\n");
    free(output);

    output = NULL;
    output_size = 0;
    fixture->lines[2].sock = (SOCKET)1;
    fixture->lines[2].ipad = strdup("127.0.0.1");
    fixture->lines[2].rcve = 1;
    fixture->lines[2].xmte = 1;
    fixture->lines[2].rxcnt = 7;
    fixture->lines[2].txcnt = 9;
    assert_non_null(fixture->lines[2].ipad);
    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);
    assert_int_equal(tmxr_show_cstat(stream, NULL, 0, &fixture->mux), SCPE_OK);
    assert_int_equal(fclose(stream), 0);
    assert_non_null(strstr(output, "Line 2:"));
    assert_non_null(strstr(output, "input (on)"));
    assert_non_null(strstr(output, "7"));
    assert_non_null(strstr(output, "9"));
    free(output);
}

/* Verify line logging state can be enabled, reported, and disabled, and that
   the mux attach string is regenerated for the owning unit. */
static void test_tmxr_log_configuration_updates_mux_attach_string(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    char *log_path = make_temp_log_path();
    char *output = NULL;
    size_t output_size = 0;
    FILE *stream;

    fixture->unit.filename = strdup("stale");
    assert_non_null(fixture->unit.filename);

    assert_int_equal(
        tmxr_set_log(NULL, 1, log_path, &fixture->mux), SCPE_OK);
    assert_non_null(fixture->lines[1].txlog);
    assert_non_null(fixture->lines[1].txlogname);
    assert_string_equal(fixture->lines[1].txlogname, log_path);
    assert_non_null(fixture->unit.filename);
    assert_non_null(strstr(fixture->unit.filename, "Line=1,Log="));
    assert_non_null(strstr(fixture->unit.filename, log_path));

    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);
    assert_int_equal(tmxr_show_log(stream, NULL, 1, &fixture->mux), SCPE_OK);
    assert_int_equal(fclose(stream), 0);
    assert_non_null(strstr(output, log_path));
    free(output);

    assert_int_equal(tmxr_set_nolog(NULL, 1, NULL, &fixture->mux), SCPE_OK);
    assert_null(fixture->lines[1].txlog);
    assert_null(fixture->lines[1].txlogname);
    assert_null(fixture->unit.filename);

    output = NULL;
    output_size = 0;
    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);
    assert_int_equal(tmxr_show_log(stream, NULL, 1, &fixture->mux), SCPE_OK);
    assert_int_equal(fclose(stream), 0);
    assert_string_equal(output, "no logging");
    free(output);

    unlink(log_path);
    free(log_path);
}

/* Verify attach help reflects single-line and multi-line mux characteristics
   through the device help context. */
static void test_tmxr_attach_help_formats_single_and_multi_line_modes(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    char *output = NULL;
    size_t output_size = 0;
    FILE *stream;

    fixture->device.help_ctx = &fixture->mux;

    fixture->mux.lines = 1;
    fixture->mux.port_speed_control = true;
    fixture->mux.modem_control = true;
    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);
    assert_int_equal(
        tmxr_attach_help(stream, &fixture->device, &fixture->unit, 0, NULL),
        SCPE_OK);
    assert_int_equal(fclose(stream), 0);
    assert_non_null(strstr(output, "TMXR Multiplexer Attach Help"));
    assert_non_null(strstr(output, "full modem control device"));
    assert_non_null(strstr(output, "ATTACH TMXR Connect=ser0"));
    assert_non_null(strstr(output, "SPEED=*8"));
    free(output);

    output = NULL;
    output_size = 0;
    fixture->mux.lines = 4;
    fixture->mux.port_speed_control = false;
    fixture->mux.modem_control = false;
    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);
    assert_int_equal(
        tmxr_attach_help(stream, &fixture->device, &fixture->unit, 0, NULL),
        SCPE_OK);
    assert_int_equal(fclose(stream), 0);
    assert_non_null(strstr(output, "Line buffering for all 4 lines"));
    assert_non_null(strstr(output, "ATTACH TMXR Line=n,Loopback"));
    assert_non_null(strstr(output, "ATTACH TMXR Line=n,Disable"));
    assert_non_null(strstr(output, "Valid line numbers are from 0 thru 3"));
    free(output);
}

/* Verify dev:line lookup returns the expected open TMXR line descriptor. */
static void test_tmxr_locate_line_finds_registered_open_line(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = NULL;

    register_and_open_test_tmxr(fixture);

    assert_int_equal(tmxr_locate_line("TMXR:2", &line), SCPE_OK);
    assert_ptr_equal(line, &fixture->lines[2]);

    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify dev:line lookup returns the SEND and EXPECT contexts for that line. */
static void test_tmxr_locate_line_send_expect_return_line_contexts(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    SEND *snd = NULL;
    EXPECT *exp = NULL;

    register_and_open_test_tmxr(fixture);

    assert_int_equal(tmxr_locate_line_send("TMXR:1", &snd), SCPE_OK);
    assert_ptr_equal(snd, &fixture->lines[1].send);
    assert_int_equal(tmxr_locate_line_expect("TMXR:3", &exp), SCPE_OK);
    assert_ptr_equal(exp, &fixture->lines[3].expect);

    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify dev:line lookup rejects unknown devices and out-of-range line
   numbers. */
static void test_tmxr_locate_line_rejects_invalid_target(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = NULL;
    SEND *snd = NULL;
    EXPECT *exp = NULL;

    register_and_open_test_tmxr(fixture);

    assert_int_equal(tmxr_locate_line("BOGUS:1", &line), SCPE_ARG);
    assert_null(line);
    assert_int_equal(tmxr_locate_line("TMXR:4", &line), SCPE_ARG);
    assert_null(line);
    assert_int_equal(tmxr_locate_line_send("TMXR:4", &snd), SCPE_ARG);
    assert_null(snd);
    assert_int_equal(tmxr_locate_line_expect("TMXR:4", &exp), SCPE_ARG);
    assert_null(exp);

    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify the SEND and EXPECT line-name helpers resolve registered open mux
   lines to stable dev:line names. */
static void test_tmxr_send_expect_line_name_formats_registered_lines(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;

    register_and_open_test_tmxr(fixture);

    assert_string_equal(tmxr_send_line_name(&fixture->lines[2].send),
                        "TMXR:2");
    assert_string_equal(tmxr_expect_line_name(&fixture->lines[1].expect),
                        "TMXR:1");

    fixture->mux.lines = 1;
    assert_string_equal(tmxr_send_line_name(&fixture->lines[0].send),
                        "TMXR");
    fixture->mux.lines = 4;

    assert_int_equal(tmxr_close_master(&fixture->mux), SCPE_OK);
}

/* Verify the line connection-order display handles both the sequential
   sentinel and compressed explicit ranges. */
static void test_tmxr_show_lnorder_formats_sequential_and_ranges(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    char *output = NULL;
    size_t output_size = 0;
    FILE *stream;

    fixture->mux.lnorder[0] = -1;
    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);
    assert_int_equal(tmxr_show_lnorder(stream, NULL, 0, &fixture->mux), SCPE_OK);
    assert_int_equal(fclose(stream), 0);
    assert_string_equal(output, "Order=0-3\n");
    free(output);

    output = NULL;
    output_size = 0;
    fixture->mux.lnorder[0] = 0;
    fixture->mux.lnorder[1] = 1;
    fixture->mux.lnorder[2] = 3;
    fixture->mux.lnorder[3] = -1;
    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);
    assert_int_equal(tmxr_show_lnorder(stream, NULL, 0, &fixture->mux), SCPE_OK);
    assert_int_equal(fclose(stream), 0);
    assert_string_equal(output, "Order=0-1;3\n");
    free(output);
}

/* Verify show-lnorder reports undefined connection-order support. */
static void test_tmxr_show_lnorder_rejects_missing_array(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;

    fixture->mux.lnorder = NULL;
    assert_int_equal(tmxr_show_lnorder(stdout, NULL, 0, &fixture->mux), SCPE_NXPAR);
}

/* Verify connection reporting covers outgoing network, incoming network,
   and serial cases without requiring a live host socket. */
static void test_tmxr_fconns_formats_connecting_and_serial_states(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    char *output = NULL;
    size_t output_size = 0;
    FILE *stream;

    fixture->lines[0].connecting = (SOCKET)1;
    fixture->lines[0].destination = strdup("remote:1234");
    fixture->lines[0].cnms = 1;
    assert_non_null(fixture->lines[0].destination);
    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);
    tmxr_fconns(stream, &fixture->lines[0], 0);
    assert_int_equal(fclose(stream), 0);
    assert_non_null(strstr(output, "line 0: "));
    assert_non_null(strstr(output, "Connection to remote port remote:1234"));
    assert_non_null(strstr(output, "Connecting"));
    free(output);

    output = NULL;
    output_size = 0;
    memset(&fixture->lines[1], 0, sizeof(fixture->lines[1]));
    fixture->lines[1].mp = &fixture->mux;
    fixture->lines[1].dptr = &fixture->device;
    fixture->lines[1].rxbsz = 16;
    fixture->lines[1].txbsz = 16;
    fixture->lines[1].connecting = (SOCKET)1;
    fixture->lines[1].ipad = strdup("127.0.0.1");
    assert_non_null(fixture->lines[1].ipad);
    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);
    tmxr_fconns(stream, &fixture->lines[1], 1);
    assert_int_equal(fclose(stream), 0);
    assert_non_null(strstr(output, "Connection from IP address 127.0.0.1"));
    free(output);

    output = NULL;
    output_size = 0;
    memset(&fixture->lines[2], 0, sizeof(fixture->lines[2]));
    fixture->lines[2].mp = &fixture->mux;
    fixture->lines[2].dptr = &fixture->device;
    fixture->lines[2].rxbsz = 16;
    fixture->lines[2].txbsz = 16;
    fixture->lines[2].serport = (SERHANDLE)(uintptr_t)1;
    fixture->lines[2].destination = strdup("ser0");
    fixture->lines[2].modem_control = true;
    fixture->lines[2].modembits = TMXR_MDM_DTR | TMXR_MDM_CTS;
    assert_non_null(fixture->lines[2].destination);
    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);
    tmxr_fconns(stream, &fixture->lines[2], 2);
    assert_int_equal(fclose(stream), 0);
    assert_non_null(strstr(output, "Connected to serial port ser0"));
    assert_non_null(strstr(output, "Modem Bits: DTR CTS"));
    free(output);
}

/* Verify statistics reporting covers connected counters and buffered
   accounting. */
static void test_tmxr_fstats_formats_connected_counters(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];
    char *output = NULL;
    size_t output_size = 0;
    FILE *stream;

    line->connecting = (SOCKET)1;
    line->rcve = 1;
    line->xmte = 1;
    line->rxbpr = 1;
    line->rxbpi = 4;
    line->rxcnt = 7;
    line->rxpcnt = 2;
    ensure_line_tx_buffer(line, 64);
    line->txbpr = 0;
    line->txbpi = 5;
    line->txcnt = 9;
    line->txpcnt = 3;
    line->txppsize = 8;
    line->txppoffset = 3;
    line->txbfd = true;
    line->txbsz = 64;
    line->txdrp = 4;
    line->txstall = 5;
    line->rxbps = 9600;
    line->txbps = 9600;
    line->bpsfactor = 2.0;

    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);
    tmxr_fstats(stream, line, 0);
    assert_int_equal(fclose(stream), 0);
    assert_non_null(strstr(output, "Line 0:"));
    assert_non_null(strstr(output, "input (on) queued/total = 1/7"));
    assert_non_null(strstr(output, "packets = 2"));
    assert_non_null(strstr(output, "output (on) queued/total = 5/9"));
    assert_non_null(strstr(output, "packet data queued/packets sent = 5/3"));
    assert_non_null(strstr(output, "speed = 9600*2 bps"));
    assert_non_null(strstr(output, "output buffer size = 64"));
    assert_non_null(strstr(output, "bytes in buffer = 5"));
    assert_non_null(strstr(output, "dropped = 4"));
    assert_non_null(strstr(output, "stalled = 5"));
    free(output);
}

/* Verify mux-wide Telnet mode toggles propagate to all lines and reject
   changes while a line is attached. */
static void test_tmxr_notelnet_mode_toggles_and_rejects_attached_lines(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;

    assert_int_equal(tmxr_set_notelnet(&fixture->mux), SCPE_OK);
    assert_true(fixture->mux.notelnet);
    assert_true(fixture->lines[0].notelnet);
    assert_true(fixture->lines[3].notelnet);

    assert_int_equal(tmxr_clear_notelnet(&fixture->mux), SCPE_OK);
    assert_false(fixture->mux.notelnet);
    assert_false(fixture->lines[0].notelnet);
    assert_false(fixture->lines[3].notelnet);

    fixture->lines[2].connecting = (SOCKET)(uintptr_t)1;
    assert_int_equal(tmxr_set_notelnet(&fixture->mux), SCPE_ALATT);
    assert_false(fixture->mux.notelnet);
}

/* Verify mux-wide connect-message mode toggles propagate to all lines and
   reject changes while a line is attached. */
static void test_tmxr_nomessage_mode_toggles_and_rejects_attached_lines(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;

    assert_int_equal(tmxr_set_nomessage(&fixture->mux), SCPE_OK);
    assert_true(fixture->mux.nomessage);
    assert_true(fixture->lines[0].nomessage);
    assert_true(fixture->lines[3].nomessage);

    assert_int_equal(tmxr_clear_nomessage(&fixture->mux), SCPE_OK);
    assert_false(fixture->mux.nomessage);
    assert_false(fixture->lines[0].nomessage);
    assert_false(fixture->lines[3].nomessage);

    fixture->lines[1].serport = (SERHANDLE)(uintptr_t)1;
    assert_int_equal(tmxr_set_nomessage(&fixture->mux), SCPE_ALATT);
    assert_false(fixture->mux.nomessage);
}

/* Verify port-speed-control state changes are allowed before attach and
   rejected once the mux is attached.  Line-specific setters must also
   validate the requested line. */
static void test_tmxr_port_speed_control_setters_update_expected_state(
    void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    char *output = NULL;
    size_t output_size = 0;
    FILE *stream;

    assert_int_equal(tmxr_set_port_speed_control(&fixture->mux), SCPE_OK);
    assert_true(fixture->mux.port_speed_control);
    assert_true(fixture->lines[0].port_speed_control);
    assert_true(fixture->lines[3].port_speed_control);

    assert_int_equal(tmxr_clear_port_speed_control(&fixture->mux), SCPE_OK);
    assert_false(fixture->mux.port_speed_control);
    assert_false(fixture->lines[0].port_speed_control);
    assert_false(fixture->lines[3].port_speed_control);

    assert_int_equal(tmxr_set_line_port_speed_control(&fixture->mux, 2), SCPE_OK);
    assert_true(fixture->lines[2].port_speed_control);
    assert_int_equal(
        tmxr_clear_line_port_speed_control(&fixture->mux, 2), SCPE_OK);
    assert_false(fixture->lines[2].port_speed_control);

    assert_int_equal(
        tmxr_set_line_port_speed_control(&fixture->mux, 4),
        SCPE_ARG | SCPE_NOMESSAGE);

    fixture->unit.flags |= UNIT_ATT;

    assert_int_equal(
        SCPE_BARE_STATUS(tmxr_set_port_speed_control(&fixture->mux)),
        SCPE_ALATT);
    fixture->unit.flags &= ~UNIT_ATT;
    assert_int_equal(tmxr_set_port_speed_control(&fixture->mux), SCPE_OK);
    fixture->unit.flags |= UNIT_ATT;
    assert_int_equal(
        SCPE_BARE_STATUS(tmxr_clear_port_speed_control(&fixture->mux)),
        SCPE_ALATT);
    assert_int_equal(
        SCPE_BARE_STATUS(
            tmxr_set_line_port_speed_control(&fixture->mux, 2)),
        SCPE_ALATT);
    assert_int_equal(
        SCPE_BARE_STATUS(
            tmxr_clear_line_port_speed_control(&fixture->mux, 2)),
        SCPE_ALATT);

    stream = open_memstream(&output, &output_size);
    assert_non_null(stream);
    assert_int_equal(tmxr_show_lines(stream, NULL, 0, &fixture->mux), SCPE_OK);
    assert_int_equal(fclose(stream), 0);
    assert_string_equal(output, "lines=4");
    free(output);
}

/* Verify TMXR debug text mode formats printable, Telnet, and octal byte
   escapes into the shared debug stream. */
static void test_tmxr_debug_formats_telnet_and_octal_text(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];
    char buffer[] = {'A', '\r', '\n', 0177};
    char *output;

    fixture->device.dctrl = TMXR_DBG_RCV;
    line->notelnet = false;

    output = capture_tmxr_debug_output(TMXR_DBG_RCV, line, "recv",
                                       buffer, (int)sizeof(buffer));
    assert_non_null(strstr(output, "Line:0 recv 4 bytes"));
    assert_non_null(strstr(output, "A_TN_CR__TN_LF__\\177_"));
    free(output);
}

/* Verify TMXR debug text mode does not read past truncated Telnet command
   sequences at the end of the debug buffer. */
static void test_tmxr_debug_formats_truncated_telnet_options(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[0];
    char trailing_iac[] = {(char)0xFF, (char)0xFB, (char)0x03};
    char trailing_will[] = {(char)0xFF, (char)0xFB, (char)0x03};
    char complete_will[] = {(char)0xFF, (char)0xFB, (char)0x03};
    char *output;

    fixture->device.dctrl = TMXR_DBG_RCV;
    line->notelnet = false;

    output = capture_tmxr_debug_output(TMXR_DBG_RCV, line, "recv",
                                       trailing_iac, 1);
    assert_non_null(strstr(output, "Line:0 recv 1 bytes"));
    assert_non_null(strstr(output, "_TN_IAC_"));
    assert_null(strstr(output, "_TN_WILL_"));
    assert_null(strstr(output, "_TN_SGA_"));
    free(output);

    output = capture_tmxr_debug_output(TMXR_DBG_RCV, line, "recv",
                                       trailing_will, 2);
    assert_non_null(strstr(output, "Line:0 recv 2 bytes"));
    assert_non_null(strstr(output, "_TN_IAC__TN_WILL_"));
    assert_null(strstr(output, "_TN_SGA_"));
    free(output);

    output = capture_tmxr_debug_output(TMXR_DBG_RCV, line, "recv",
                                       complete_will,
                                       (int)sizeof(complete_will));
    assert_non_null(strstr(output, "Line:0 recv 3 bytes"));
    assert_non_null(strstr(output, "_TN_IAC__TN_WILL__TN_SGA_"));
    free(output);
}

/* Verify TMXR debug hex-dump mode formats non-Telnet data as grouped hex
   plus printable text. */
static void test_tmxr_debug_formats_notelnet_hex_dump(void **state)
{
    struct sim_tmxr_fixture *fixture = *state;
    TMLN *line = &fixture->lines[1];
    char buffer[] = {'A', 001, 'B'};
    char *output;

    fixture->device.dctrl = TMXR_DBG_RCV;
    line->notelnet = true;

    output = capture_tmxr_debug_output(TMXR_DBG_RCV, line, "recv",
                                       buffer, (int)sizeof(buffer));
    assert_non_null(strstr(output, "Line:1 0000"));
    assert_non_null(strstr(output, "41 01 42"));
    assert_non_null(strstr(output, "A.B"));
    free(output);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_tmxr_loopback_toggle_updates_buffer_state,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(test_tmxr_halfduplex_toggle_round_trips,
                                        setup_sim_tmxr_fixture,
                                        teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_queue_length_helpers_handle_wraparound,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_connection_poll_interval_validates_zero,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_set_line_speed_updates_timing_state,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_set_line_speed_factor_only_reuses_current_bps,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_set_line_speed_rejects_invalid_input,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_modem_control_passthru_toggles_and_rejects_attached_mux,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_set_line_units_update_poll_state,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_line_mode_setters_toggle_state,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_set_config_line_updates_cached_config_and_speed,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_set_config_line_rejects_invalid_config,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_buffered_defaults_to_32768,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_listener_sets_mux_listener_state,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_buffered_accepts_explicit_size,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_missing_line_specifier,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_invalid_line_specifier,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_show_sync_reports_missing_network_support,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_line_listener_sets_line_state,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_invalid_listener_acl,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_invalid_listener_option,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_translates_sync_aliases_via_eth_inventory,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_poll_rx_ignores_short_framer_status_frame,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_poll_rx_skips_short_framer_data_frame,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_put_packet_ln_reports_framer_write_failure,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_start_framer_uses_prewrite_status_snapshot,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_start_framer_skips_status_wait_after_write_failure,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_stop_framer_skips_status_wait_after_write_failure,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_show_sync_lists_framer_aliases,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_loopback_sets_line_state,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_close_master_clears_loopback_buffers,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_invalid_buffered_size,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_disabled_marks_line_unavailable,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_missing_connect_specifier,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_missing_sync_specifier,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_invalid_speed_specifier,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_programmatic_speed_override,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_restores_speed_with_port_speed_control,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_connect_sets_destination_and_ipad,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_ambiguous_destination,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_datagram_connect_without_listen,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_line_listener_when_mux_listener_exists,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_framer_without_line,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_missing_sync_alias,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_non_pcap_sync_alias,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_connect_with_sync,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_datagram_telnet_destination,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_unexpected_connect_option,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_disabled_with_destination,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_datagram_connect_uses_listen_source,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_verbose_serial_attach_uses_sleep_hook,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_invalid_framer_mode,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_invalid_framer_speed,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
#if CBUFSIZE > (ETH_DEV_NAME_MAX + 32)
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_oversize_framer_device_name,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
#endif
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_disabled_without_line,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_open_master_rejects_ambiguous_loopback,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_poll_conn_initiates_outgoing_connection_via_hook,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_poll_conn_accepts_line_listener_connection,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_poll_conn_marks_outgoing_connection_established,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_poll_conn_retries_failed_outgoing_connection,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_poll_conn_reports_mux_listener_all_busy,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_poll_conn_rings_mux_listener_line_with_dtr_low,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_poll_conn_times_out_ringing_mux_listener,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_poll_conn_rejects_line_listener_unexpected_source,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_close_ln_closes_serial_and_clears_cached_state,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_reset_ln_pulses_serial_control_lines,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_reset_ln_restarts_configured_outgoing_destination,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_set_get_modem_bits_raises_dtr_and_starts_connect,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_detach_clears_attached_state_and_line_poll_flags,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_connection_message_honors_suppression_flags,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_connection_message_formats_multiline_banner,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_connection_message_formats_single_line_forced_banner,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_line_attach_string_returns_null_for_unconfigured_line,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_line_attach_string_formats_listener_state,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_line_attach_string_formats_serial_destination,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_putc_ln_reports_lost_output_on_disconnected_line,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_putc_ln_duplicates_telnet_iac,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_putc_ln_loopback_round_trips_through_buffered_send,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_poll_rx_notelnet_uses_full_small_buffer,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_put_packet_ln_stalls_when_packet_transmit_is_busy,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_get_packet_ln_ex_decodes_framed_buffer,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_getc_ln_reads_manual_buffered_byte,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_put_packet_ln_loopback_without_conn_round_trips,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_get_packet_ln_reports_empty_and_lost_states,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_poll_tx_drains_loopback_queue_and_reenables_xmte,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_txdone_ln_tracks_pending_and_completed_output,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_set_lnorder_fills_unspecified_lines_with_all,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_set_lnorder_rejects_trailing_items_after_all,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(test_tmxr_linemsg_buffers_plain_text,
                                        setup_sim_tmxr_fixture,
                                        teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_linemsgf_expands_newlines_to_crlf,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_linemsgf_handles_long_formatted_output,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_show_summ_formats_single_and_multi_line_state,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_show_cstat_formats_disconnected_and_connected_states,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_log_configuration_updates_mux_attach_string,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_attach_help_formats_single_and_multi_line_modes,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_locate_line_finds_registered_open_line,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_locate_line_send_expect_return_line_contexts,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_locate_line_rejects_invalid_target,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_send_expect_line_name_formats_registered_lines,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_show_lnorder_formats_sequential_and_ranges,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_show_lnorder_rejects_missing_array,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_fconns_formats_connecting_and_serial_states,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_fstats_formats_connected_counters,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_notelnet_mode_toggles_and_rejects_attached_lines,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_nomessage_mode_toggles_and_rejects_attached_lines,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_port_speed_control_setters_update_expected_state,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_debug_formats_telnet_and_octal_text,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_debug_formats_truncated_telnet_options,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
        cmocka_unit_test_setup_teardown(
            test_tmxr_debug_formats_notelnet_hex_dump,
            setup_sim_tmxr_fixture, teardown_sim_tmxr_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
