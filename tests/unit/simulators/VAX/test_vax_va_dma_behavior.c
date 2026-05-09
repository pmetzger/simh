#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "vax_2681.h"
#include "vax_defs.h"
#include "vax_gpx.h"
#include "vax_lk.h"
#include "vax_va_internal.h"
#include "vax_vs.h"

#define DGA_MODE_BTP (2u << 9)
#define DGA_MODE_PTB (3u << 9)
#define DMA_REQUEST_BYTES 128u

extern uint32 va_dga_addr;
extern uint32 va_dga_count;
extern uint32 va_dga_csr;
extern uint32 va_dga_int;
extern t_stat va_dmasvc(UNIT *uptr);
extern UNIT va_unit[];

int32 int_req[IPL_HLVL];
int32 sys_model;
uint32 fault_PC;
uint32 trpirq;
int32 hlt_pin;
int32 tmxr_poll = 10000;
jmp_buf save_env;

int32 va_adp[ADP_NUMREG];

static int32 fake_map_read_residual;
static int32 fake_map_write_residual;

/*
 * Reset only the state touched by these DGA DMA tests. The full QDSS reset
 * opens video resources and is intentionally outside this unit-test boundary.
 */
static void reset_dga_dma_state(void)
{
    memset(int_req, 0, sizeof(int_req));
    memset(va_adp, 0, sizeof(va_adp));
    va_adp[ADP_STAT] = ADPSTAT_IRR | ADPSTAT_ITR;
    va_dga_addr = 0x1200;
    va_dga_count = DMA_REQUEST_BYTES;
    va_dga_csr = 0;
    va_dga_int = 0;
    fake_map_read_residual = 0;
    fake_map_write_residual = 0;
}

/*
 * A Qbus bus timeout before the first BTP word must not make the DGA claim
 * that the transfer completed. The address and byte counter remain at the
 * failed transfer point, and the CSR records the bus timeout.
 */
static void test_btp_immediate_bus_timeout_preserves_dma_position(void **state)
{
    (void)state;

    reset_dga_dma_state();
    va_dga_csr = DGA_MODE_BTP;
    fake_map_write_residual = DMA_REQUEST_BYTES;

    assert_int_equal(va_dmasvc(&va_unit[1]), SCPE_OK);

    assert_int_equal(va_dga_addr, 0x1200);
    assert_int_equal(va_dga_count, DMA_REQUEST_BYTES);
    assert_int_equal(va_dga_csr & VA_DGA_CSR_DMA_ERROR, VA_DGA_CSR_DMA_ERROR);
    assert_int_equal(va_dga_csr & VA_DGA_CSR_BUS_TIMEOUT_ERR,
                     VA_DGA_CSR_BUS_TIMEOUT_ERR);
    assert_int_equal(va_dga_int & 0x1000, 0x1000);
}

/*
 * On a successful BTP write, the fix must preserve existing behavior: the
 * whole request is consumed, no error bits are set, and the normal DMA
 * interrupt is posted when the transfer completes.
 */
static void test_btp_success_consumes_full_dma_request(void **state)
{
    (void)state;

    reset_dga_dma_state();
    va_dga_csr = DGA_MODE_BTP;

    assert_int_equal(va_dmasvc(&va_unit[1]), SCPE_OK);

    assert_int_equal(va_dga_addr, 0x1280);
    assert_int_equal(va_dga_count, 0);
    assert_int_equal(va_dga_csr & VA_DGA_CSR_ERROR_BITS, 0);
    assert_int_equal(va_dga_int & 0x1000, 0x1000);
}

/*
 * A partial PTB read means only the successfully mapped words are available
 * for processing. The DGA must advance by that partial byte count and retain
 * the residual count for software to observe.
 */
static void
test_ptb_partial_bus_timeout_advances_only_transferred_bytes(void **state)
{
    (void)state;

    reset_dga_dma_state();
    va_dga_csr = DGA_MODE_PTB;
    fake_map_read_residual = 64;

    assert_int_equal(va_dmasvc(&va_unit[1]), SCPE_OK);

    assert_int_equal(va_dga_addr, 0x1240);
    assert_int_equal(va_dga_count, 64);
    assert_int_equal(va_dga_csr & VA_DGA_CSR_DMA_ERROR, VA_DGA_CSR_DMA_ERROR);
    assert_int_equal(va_dga_csr & VA_DGA_CSR_BUS_TIMEOUT_ERR,
                     VA_DGA_CSR_BUS_TIMEOUT_ERR);
    assert_int_equal(va_dga_int & 0x1000, 0x1000);
}

/*
 * On a successful PTB read, the DGA still consumes the full mapped byte range.
 * This locks in the success-path behavior that existed before the warning fix.
 */
static void test_ptb_success_consumes_full_dma_request(void **state)
{
    (void)state;

    reset_dga_dma_state();
    va_dga_csr = DGA_MODE_PTB;

    assert_int_equal(va_dmasvc(&va_unit[1]), SCPE_OK);

    assert_int_equal(va_dga_addr, 0x1280);
    assert_int_equal(va_dga_count, 0);
    assert_int_equal(va_dga_csr & VA_DGA_CSR_ERROR_BITS, 0);
    assert_int_equal(va_dga_int & 0x1000, 0x1000);
}

int32 Map_ReadW(uint32 ba, int32 bc, uint16 *buf)
{
    (void)ba;
    (void)bc;
    (void)buf;
    return fake_map_read_residual;
}

int32 Map_WriteW(uint32 ba, int32 bc, const uint16 *buf)
{
    (void)ba;
    (void)bc;
    (void)buf;
    return fake_map_write_residual;
}

uint32 va_fifo_rd(void)
{
    return 0;
}

void va_fifo_wr(uint32 val)
{
    (void)val;
}

t_stat va_btp(UNIT *uptr, bool zmode)
{
    (void)uptr;
    (void)zmode;
    return SCPE_OK;
}

t_stat va_ptb(UNIT *uptr, bool zmode)
{
    (void)uptr;
    (void)zmode;
    return SCPE_OK;
}

int32 va_adp_rd(int32 rg)
{
    (void)rg;
    return 0;
}

void va_adp_wr(int32 rg, int32 val)
{
    (void)rg;
    (void)val;
}

t_stat va_adp_reset(DEVICE *dptr)
{
    (void)dptr;
    return SCPE_OK;
}

t_stat va_adp_svc(UNIT *uptr)
{
    (void)uptr;
    return SCPE_OK;
}

void va_adpstat(uint32 set, uint32 clr)
{
    va_adp[ADP_STAT] |= set;
    va_adp[ADP_STAT] &= ~clr;
}

t_stat lk_rd(uint8 *c)
{
    (void)c;
    return SCPE_OK;
}

t_stat lk_wr(uint8 c)
{
    (void)c;
    return SCPE_OK;
}

void lk_event(SIM_KEY_EVENT *ev)
{
    (void)ev;
}

t_stat vs_rd(uint8 *c)
{
    (void)c;
    return SCPE_OK;
}

t_stat vs_wr(uint8 c)
{
    (void)c;
    return SCPE_OK;
}

void vs_event(SIM_MOUSE_EVENT *ev)
{
    (void)ev;
}

void ua2681_wr(UART2681 *ctx, uint32 rg, uint32 data)
{
    (void)ctx;
    (void)rg;
    (void)data;
}

uint32 ua2681_rd(UART2681 *ctx, uint32 rg)
{
    (void)ctx;
    (void)rg;
    return 0;
}

t_stat ua2681_svc(UART2681 *ctx)
{
    (void)ctx;
    return SCPE_OK;
}

t_stat ua2681_reset(UART2681 *ctx)
{
    (void)ctx;
    return SCPE_OK;
}

void ua2681_ip0_wr(UART2681 *ctx, uint32 set)
{
    (void)ctx;
    (void)set;
}

void ua2681_ip1_wr(UART2681 *ctx, uint32 set)
{
    (void)ctx;
    (void)set;
}

void ua2681_ip2_wr(UART2681 *ctx, uint32 set)
{
    (void)ctx;
    (void)set;
}

void ua2681_ip3_wr(UART2681 *ctx, uint32 set)
{
    (void)ctx;
    (void)set;
}

uint8 ua2681_oport(UART2681 *ctx)
{
    (void)ctx;
    return 0;
}

int32 eval_int(void)
{
    return 0;
}

t_stat auto_config(const char *name, int32 n)
{
    (void)name;
    (void)n;
    return SCPE_OK;
}

t_stat set_addr(UNIT *uptr, int32 val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;
    return SCPE_OK;
}

t_stat show_addr(FILE *st, UNIT *uptr, int32 val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;
    return SCPE_OK;
}

t_stat set_vec(UNIT *uptr, int32 val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;
    return SCPE_OK;
}

t_stat show_vec(FILE *st, UNIT *uptr, int32 val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;
    return SCPE_OK;
}

t_stat cpu_set_model(UNIT *uptr, int32 val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;
    return SCPE_OK;
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_btp_immediate_bus_timeout_preserves_dma_position),
        cmocka_unit_test(test_btp_success_consumes_full_dma_request),
        cmocka_unit_test(
            test_ptb_partial_bus_timeout_advances_only_transferred_bytes),
        cmocka_unit_test(test_ptb_success_consumes_full_dma_request),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
