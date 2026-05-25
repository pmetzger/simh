#include <setjmp.h>
#include <stdint.h>

#include "test_cmocka.h"

#include "vax_defs.h"

uint32_t R[16];
uint32_t PSL;
uint32_t SISR;
uint32_t fault_PC;
uint32_t p1;
uint32_t p2;
uint32_t trpirq;
uint32_t mchk_ref;
uint32_t *M;
int32_t hlt_pin;
int32_t crd_err;
int32_t cur_cpu;
int32_t cpu_msk = 1;
int32_t in_ie;
#ifdef SIM_ASYNCH_IO
int32_t sim_asynch_latency = 4000;
int32_t sim_asynch_inst_latency = 20;
#endif
jmp_buf save_env;
DEVICE cpu_dev;
UNIT cpu_unit;

static int test_tick_ack_count;
static int32_t test_tick_ack_delay;
static int32_t test_tick_ack_clock;

/* Satisfy TODR write-through-cache hooks that are linked in by the included
   standard-device implementation but are not used by these timer tests. */
void wtc_set_valid(void) {}
void wtc_set_invalid(void) {}
t_stat wtc_set(UNIT *uptr, int32_t val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;

    return SCPE_OK;
}

#define sim_rtcn_tick_ack test_sim_rtcn_tick_ack

/* Capture 8200 timer acknowledgements without involving the shared runtime
   timer state, so the tests can distinguish writes that should and should not
   tell the runtime that the guest accepted a generated tick. */
static t_stat test_sim_rtcn_tick_ack(uint32_t delay, int32_t clock)
{
    test_tick_ack_count++;
    test_tick_ack_delay = (int32_t)delay;
    test_tick_ack_clock = clock;

    return SCPE_OK;
}

#include "vax820_stddev.c"

/* Put just the interval timer state back into a known stopped configuration.
   The test includes vax820_stddev.c directly, so its static timer registers
   are visible here without exporting production test hooks. */
static void reset_timer_state(void)
{
    tmr_iccs = 0;
    tmr_icr = 0;
    tmr_nicr = 0xFFFFD8F0;
    tmr_int = 0;
    test_tick_ack_count = 0;
    test_tick_ack_delay = -1;
    test_tick_ack_clock = -1;
    sim_cancel(&tmr_unit);
}

/* ICCS bit 7 is set by hardware when ICR overflows, and MTPR writes of 1
   clear that pending interrupt bit.  The simulator timing service should only
   be told that the guest digested a generated tick when the 8200 timer had a
   pending DONE bit before the write; otherwise software that defensively
   writes DONE while enabling the clock can falsely enable catch-up ticks. */
static void test_done_clear_without_pending_tick_is_not_ack(void **state)
{
    (void)state;

    reset_timer_state();

    iccs_wr(TMR_CSR_DON | TMR_CSR_IE | TMR_CSR_RUN);

    assert_int_equal(test_tick_ack_count, 0);
    assert_false(tmr_iccs & TMR_CSR_DON);
    assert_true(tmr_iccs & TMR_CSR_IE);
    assert_true(tmr_iccs & TMR_CSR_RUN);
}

/* A write that clears an actually pending DONE bit represents the guest
   accepting the most recent timer interrupt.  That is the point where the
   runtime timer service may schedule catch-up ticks if host time has fallen
   behind the guest's 100 Hz interval clock. */
static void test_done_clear_with_pending_tick_acknowledges_tick(void **state)
{
    (void)state;

    reset_timer_state();
    tmr_iccs = TMR_CSR_DON | TMR_CSR_IE | TMR_CSR_RUN;

    iccs_wr(TMR_CSR_DON | TMR_CSR_IE | TMR_CSR_RUN);

    assert_int_equal(test_tick_ack_count, 1);
    assert_int_equal(test_tick_ack_delay, 20);
    assert_int_equal(test_tick_ack_clock, TMR_CLK);
    assert_false(tmr_iccs & TMR_CSR_DON);
    assert_true(tmr_iccs & TMR_CSR_IE);
    assert_true(tmr_iccs & TMR_CSR_RUN);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_done_clear_without_pending_tick_is_not_ack),
        cmocka_unit_test(test_done_clear_with_pending_tick_acknowledges_tick),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
