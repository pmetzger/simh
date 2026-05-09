#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "scp.h"
#include "sim_defs.h"
#include "sim_timer_internal.h"
#include "sim_timer.h"
#include "test_scp_fixture.h"
#include "test_simh_personality.h"
#include "test_support.h"

static int simh_test_clock_calls = 0;
static int simh_test_sleep_calls = 0;
static int simh_test_last_clock_id = -1;
static int simh_test_clock_status = 0;
static struct timespec simh_test_last_sleep_req = {0};
static struct timespec simh_test_clock_values[4] = {{0}};
static size_t simh_test_clock_value_count = 0;
static size_t simh_test_clock_value_index = 0;
static uint64_t simh_test_clock_auto_msec = 0;
static uint32_t simh_test_clock_auto_step_msec = 0;
static int simh_test_clock_service_calls = 0;
static int simh_test_target_service_calls = 0;

struct sim_timer_activation_fixture {
    DEVICE clock_device;
    DEVICE target_device;
    DEVICE *devices[3];
    UNIT clock_unit;
    UNIT target_unit;
    bool saved_sim_is_running;
};

struct sim_timer_negative_delay_context {
    struct sim_timer_activation_fixture *fixture;
    t_stat status;
};

struct sim_timer_dynamic_throttle_case {
    const char *spec;
    uint32_t type;
    uint32_t val;
    int32_t wait;
};

/* Count clock service dispatches without otherwise changing timer state. */
static t_stat sim_timer_counting_clock_svc (UNIT *uptr)
{
    (void)uptr;

    ++simh_test_clock_service_calls;
    return SCPE_OK;
}

/* Count target service dispatches without otherwise changing timer state. */
static t_stat sim_timer_counting_target_svc (UNIT *uptr)
{
    (void)uptr;

    ++simh_test_target_service_calls;
    return SCPE_OK;
}

/* Return scripted timespec values through the shared time wrapper hook. */
static int simh_test_clock_gettime_stub (int clock_id, struct timespec *tp)
{
    ++simh_test_clock_calls;
    simh_test_last_clock_id = clock_id;
    if ((tp != NULL) &&
        (simh_test_clock_value_index < simh_test_clock_value_count))
        *tp = simh_test_clock_values[simh_test_clock_value_index++];
    else if ((tp != NULL) && (simh_test_clock_auto_step_msec != 0)) {
        simh_test_clock_auto_msec += simh_test_clock_auto_step_msec;
        tp->tv_sec = (time_t)(simh_test_clock_auto_msec / 1000u);
        tp->tv_nsec = (long)((simh_test_clock_auto_msec % 1000u) * 1000000u);
    }
    if (simh_test_clock_status != 0)
        errno = EINVAL;
    return simh_test_clock_status;
}

/* Record the requested sleep interval and return success. */
static int simh_test_nanosleep_stub (const struct timespec *req,
                                     struct timespec *rem)
{
    ++simh_test_sleep_calls;
    if (req != NULL)
        simh_test_last_sleep_req = *req;
    if (rem != NULL) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    return 0;
}

/* Reset timer test hooks and scripted values between cases. */
static int setup_sim_timer_fixture (void **state)
{
    (void)state;

    simh_test_clock_calls = 0;
    simh_test_sleep_calls = 0;
    simh_test_last_clock_id = -1;
    simh_test_clock_status = 0;
    simh_test_last_sleep_req = (struct timespec){0};
    simh_test_clock_value_count = 0;
    simh_test_clock_value_index = 0;
    simh_test_clock_auto_msec = 0;
    simh_test_clock_auto_step_msec = 0;
    simh_test_clock_service_calls = 0;
    simh_test_target_service_calls = 0;
    sim_time_reset_test_hooks ();
    sim_timer_reset_test_state ();
    sim_is_running = false;
    sim_set_rom_delay_factor (1);
    errno = 0;
    return 0;
}

/* Return the current private throttle state through the internal test seam. */
static struct sim_timer_throttle_test_state sim_timer_throttle_state (void)
{
    struct sim_timer_throttle_test_state throttle;

    sim_timer_get_throttle_test_state (&throttle);
    return throttle;
}

/* Reset the scripted host clock and fake sleep call records. */
static void sim_timer_reset_scripted_host_time (void)
{
    simh_test_clock_calls = 0;
    simh_test_sleep_calls = 0;
    simh_test_last_clock_id = -1;
    simh_test_clock_status = 0;
    simh_test_last_sleep_req = (struct timespec){0};
    simh_test_clock_value_count = 0;
    simh_test_clock_value_index = 0;
    simh_test_clock_auto_msec = 0;
    simh_test_clock_auto_step_msec = 0;
}

/* Restore the default shared time hooks after a test case completes. */
static int teardown_sim_timer_fixture (void **state)
{
    (void)state;

    while (sim_clock_queue != QUEUE_LIST_END)
        sim_cancel (sim_clock_queue);
    sim_time_reset_test_hooks ();
    return 0;
}

/* Read and return the complete text written to a temporary stream. */
static char *sim_timer_read_stream_text (FILE *stream)
{
    char *text = NULL;
    size_t size = 0;

    assert_int_equal (fflush (stream), 0);
    rewind (stream);
    assert_int_equal (simh_test_read_stream (stream, &text, &size), 0);
    assert_non_null (text);
    return text;
}

/* Verify SHOW THROTTLE output includes a user-facing status fragment. */
static void sim_timer_assert_show_throt_contains (const char *expected)
{
    char *text;
    FILE *stream;

    stream = tmpfile ();
    assert_non_null (stream);
    assert_int_equal (sim_show_throt (stream, NULL, NULL, 0, NULL), SCPE_OK);
    text = sim_timer_read_stream_text (stream);
    assert_non_null (strstr (text, expected));
    free (text);
    fclose (stream);
}

/* Initialize timer host timing through deterministic clock and sleep hooks. */
static void sim_timer_init_with_deterministic_host_timing (void)
{
    simh_test_clock_auto_step_msec = 1;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub,
                             simh_test_nanosleep_stub);

    assert_false (sim_timer_init ());
}

/* Reset and initialize the timer subsystem with deterministic host timing. */
static void sim_timer_reset_initialized_host_timing (void)
{
    sim_timer_reset_test_state ();
    sim_set_rom_delay_factor (1);
    sim_timer_reset_scripted_host_time ();
    sim_time_reset_test_hooks ();
    sim_timer_init_with_deterministic_host_timing ();
    simh_test_clock_calls = 0;
    simh_test_sleep_calls = 0;
    simh_test_clock_auto_step_msec = 0;
}

/* Run the currently queued throttle event as if its delay had expired. */
static void sim_timer_process_due_throttle_event (int32_t catchup)
{
    assert_true (sim_is_active (&sim_throttle_unit));
    sim_interval = -catchup;
    assert_int_equal (sim_process_event (), SCPE_OK);
}

/* Drive calibrated dynamic throttling through deferred startup into the
   throttling state using a deterministic final timing sample. */
static void sim_timer_start_calibrated_dynamic_throttle (
    const char *spec, uint32_t final_clock_step_msec)
{
    struct sim_timer_throttle_test_state throttle;

    assert_int_equal (sim_set_throt (1, spec), SCPE_OK);
    simh_test_clock_auto_step_msec = 1;
    assert_int_equal (sim_throt_svc (&sim_throttle_unit), SCPE_OK);

    throttle = sim_timer_throttle_state ();
    assert_int_equal (throttle.state, SIM_THROT_STATE_INIT);
    assert_true (sim_is_active (&sim_throttle_unit));
    assert_int_equal (sim_activate_time (&sim_throttle_unit), 10001);

    rtcs[0].calibrations = 3;
    simh_test_clock_auto_step_msec = final_clock_step_msec;
    sim_timer_process_due_throttle_event (0);
}

/* Queue the calibrated clock assist unit at a known point in the tick. */
static void sim_timer_queue_calibrated_tick (
    struct sim_timer_activation_fixture *fixture, double usec_delay)
{
    assert_int_equal (sim_timer_activate_after (&fixture->clock_unit,
                                                usec_delay),
                      SCPE_OK);
    assert_true (sim_is_active (&sim_timer_units[0]));
}

/* Try a negative wall-clock activation through the timer API. */
static void sim_timer_try_negative_activation (void *context)
{
    struct sim_timer_negative_delay_context *negative_context = context;

    negative_context->status = sim_timer_activate_after (
        &negative_context->fixture->target_unit, -100.0);
}

/* Try a negative wall-clock activation through the public double API. */
static void sim_timer_try_negative_public_activation (void *context)
{
    struct sim_timer_negative_delay_context *negative_context = context;

    negative_context->status =
        sim_activate_after_d (&negative_context->fixture->target_unit, -100.0);
}

/* Try a negative absolute wall-clock activation through the public double API. */
static void sim_timer_try_negative_abs_public_activation (void *context)
{
    struct sim_timer_negative_delay_context *negative_context = context;

    negative_context->status = sim_activate_after_abs_d (
        &negative_context->fixture->target_unit, -100.0);
}

/* Build a calibrated-timer fixture for wall-clock activation tests. */
static int setup_sim_timer_activation_fixture (void **state)
{
    struct sim_timer_activation_fixture *fixture;

    assert_int_equal (setup_sim_timer_fixture (state), 0);

    fixture = calloc (1, sizeof (*fixture));
    assert_non_null (fixture);

    simh_test_init_device_unit (&fixture->clock_device, &fixture->clock_unit,
                                "CLK", "CLK0", 0, 0, 8, 1);
    simh_test_init_device_unit (&fixture->target_device, &fixture->target_unit,
                                "TMR", "TMR0", 0, 0, 8, 1);
    fixture->clock_unit.action = sim_timer_counting_clock_svc;
    fixture->target_unit.action = sim_timer_counting_target_svc;
    fixture->devices[0] = &fixture->clock_device;
    fixture->devices[1] = &fixture->target_device;
    fixture->devices[2] = NULL;

    assert_int_equal (
        simh_test_install_devices ("zimh-unit-sim-timer", fixture->devices), 0);
    sim_timer_init_with_deterministic_host_timing ();
    simh_test_clock_calls = 0;
    simh_test_sleep_calls = 0;
    simh_test_clock_auto_step_msec = 0;
    assert_int_equal (
        sim_rtcn_init_unit_ticks (&fixture->clock_unit, 100, 0, 100), 100);
    fixture->saved_sim_is_running = sim_is_running;

    *state = fixture;
    return 0;
}

/* Tear down timer fixture state that may be queued by activation tests. */
static int teardown_sim_timer_activation_fixture (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    sim_cancel (&fixture->target_unit);
    sim_register_clock_unit_tmr (NULL, 0);
    sim_is_running = fixture->saved_sim_is_running;
    simh_test_reset_simulator_state ();
    free (fixture);
    *state = NULL;
    assert_int_equal (teardown_sim_timer_fixture (state), 0);
    return 0;
}

/* Verify timespec subtraction borrows from seconds when nanoseconds
   would otherwise go negative. */
static void test_sim_timespec_diff_borrows_and_normalizes(void **state)
{
    struct timespec minuend = {.tv_sec = 5, .tv_nsec = 100};
    struct timespec subtrahend = {.tv_sec = 2, .tv_nsec = 200};
    struct timespec diff;

    (void)state;

    sim_timespec_diff(&diff, &minuend, &subtrahend);
    assert_int_equal(diff.tv_sec, 2);
    assert_int_equal(diff.tv_nsec, 999999900L);
}

/* Verify timespec subtraction leaves already-normalized values alone. */
static void test_sim_timespec_diff_handles_simple_difference(void **state)
{
    struct timespec minuend = {.tv_sec = 12, .tv_nsec = 800000000};
    struct timespec subtrahend = {.tv_sec = 7, .tv_nsec = 300000000};
    struct timespec diff;

    (void)state;

    sim_timespec_diff(&diff, &minuend, &subtrahend);
    assert_int_equal(diff.tv_sec, 5);
    assert_int_equal(diff.tv_nsec, 500000000L);
}

/* Verify deadline construction uses CLOCK_REALTIME and normalizes carry. */
static void test_sim_timer_deadline_msec_builds_absolute_deadline(void **state)
{
    struct timespec deadline = {0};

    (void)state;

    simh_test_clock_values[0] = (struct timespec){.tv_sec = 10,
                                                  .tv_nsec = 900000000L};
    simh_test_clock_value_count = 1;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub, NULL);

    assert_int_equal (sim_timer_deadline_msec (&deadline, 250), 0);
    assert_int_equal (simh_test_clock_calls, 1);
    assert_int_equal (simh_test_last_clock_id, CLOCK_REALTIME);
    assert_int_equal (deadline.tv_sec, 11);
    assert_int_equal (deadline.tv_nsec, 150000000L);
}

/* Verify deadline construction reports wrapper failure unchanged. */
static void test_sim_timer_deadline_msec_reports_clock_failure(void **state)
{
    struct timespec deadline = {.tv_sec = 1, .tv_nsec = 2};

    (void)state;

    simh_test_clock_status = -1;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub, NULL);

    assert_int_equal (sim_timer_deadline_msec (&deadline, 5), -1);
    assert_int_equal (simh_test_clock_calls, 1);
    assert_int_equal (deadline.tv_sec, 1);
    assert_int_equal (deadline.tv_nsec, 2);
}

/* Verify the timer library reports its uninitialized default capability
   state without requiring timer startup. */
static void test_sim_timer_idle_capable_reports_default_state(void **state)
{
    uint32_t host_sleep_ms = 1234;
    uint32_t host_tick_ms = 5678;

    (void)state;

    assert_false(sim_timer_idle_capable(&host_sleep_ms, &host_tick_ms));
    assert_int_equal(host_sleep_ms, 0);
    assert_int_equal(host_tick_ms, 0);
}

/* Verify timer initialization can be driven deterministically through the
   shared time hooks and records the measured host timing capability. */
static void test_sim_timer_init_measures_host_timing_with_hooks (void **state)
{
    uint32_t host_sleep_ms = 0;
    uint32_t host_tick_ms = 0;

    (void)state;

    simh_test_clock_auto_step_msec = 1;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub,
                             simh_test_nanosleep_stub);

    assert_false (sim_timer_init ());
    assert_true (simh_test_clock_calls > 400);
    assert_true (simh_test_sleep_calls > 200);
    assert_true (sim_timer_idle_capable (&host_sleep_ms, &host_tick_ms));
    assert_int_equal (host_sleep_ms, 1);
    assert_int_equal (host_tick_ms, 1);
    assert_ptr_equal (rtcs[0].timer_unit, &sim_timer_units[0]);
    assert_ptr_equal (rtcs[SIM_NTIMERS].timer_unit,
                      &sim_timer_units[SIM_NTIMERS]);
}

/* Verify public calibrated-timer queries return the documented defaults
   before any runtime timer initialization occurs. */
static void test_sim_timer_default_query_helpers_are_stable(void **state)
{
    (void)state;

    assert_int_equal(sim_rtcn_calibrated_tmr(), 0);
    assert_int_equal(sim_rtcn_tick_size(0), 10000);
    assert_int_equal(sim_rtcn_tick_size(SIM_NTIMERS), 10000);
    assert_true(sim_timer_inst_per_sec() >= 0.0);
}

/* Verify sim_os_msec uses the shared clock wrapper and ms conversion. */
static void test_sim_os_msec_uses_shared_clock_wrapper (void **state)
{
    (void)state;

    simh_test_clock_values[0] = (struct timespec){.tv_sec = 12,
                                                  .tv_nsec = 345000000L};
    simh_test_clock_value_count = 1;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub, NULL);

    assert_int_equal (sim_os_msec (), 12345);
    assert_int_equal (simh_test_clock_calls, 1);
    assert_int_equal (simh_test_last_clock_id, CLOCK_REALTIME);
}

/* Verify epoch-scale seconds produce the expected wrapping millisecond
   tick value. */
static void test_sim_os_msec_handles_epoch_scale_seconds (void **state)
{
    const time_t epoch_seconds = 1777949713;
    const long epoch_nsecs = 456000000L;
    const uint32_t expected =
        (uint32_t)(((uint64_t)epoch_seconds * 1000u) +
                 ((uint64_t)epoch_nsecs / 1000000u));

    (void)state;

    simh_test_clock_values[0] = (struct timespec){.tv_sec = epoch_seconds,
                                                  .tv_nsec = epoch_nsecs};
    simh_test_clock_value_count = 1;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub, NULL);

    assert_int_equal (sim_os_msec (), expected);
    assert_int_equal (simh_test_clock_calls, 1);
    assert_int_equal (simh_test_last_clock_id, CLOCK_REALTIME);
}

/* Verify invalid timer initialization requests are bounded to the sanitized
   default delay without touching out-of-range timer state. */
static void test_sim_rtcn_init_rejects_invalid_timer_numbers (void **state)
{
    (void)state;

    assert_int_equal (sim_rtcn_init_unit_ticks (NULL, 0, -1, 0), 1);
    assert_int_equal (
        sim_rtcn_init_unit_ticks (NULL, 25, SIM_NTIMERS + 1, 0), 25);
    assert_int_equal (sim_rtcn_calb (100, -1), 10000);
    assert_int_equal (sim_rtcn_calb (100, SIM_NTIMERS + 1), 10000);
}

/* Verify timer-unit initialization records the active clock contract and
   resets the one-second calibration counters. */
static void test_sim_rtcn_init_unit_ticks_records_clock_state (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;
    RTC *rtc = &rtcs[0];

    assert_ptr_equal (rtc->clock_unit, &fixture->clock_unit);
    assert_ptr_equal (rtc->timer_unit, &sim_timer_units[0]);
    assert_int_equal (fixture->clock_unit.dynflags & UNIT_TMR_UNIT,
                      UNIT_TMR_UNIT);
    assert_int_equal (rtc->ticks, 0);
    assert_int_equal (rtc->hz, 100);
    assert_int_equal (rtc->based, 100);
    assert_int_equal (rtc->currd, 100);
    assert_int_equal (rtc->initd, 100);
    assert_int_equal (rtc->elapsed, 0);
    assert_int_equal (rtc->calib_initializations, 1);
}

/* Verify legacy timer-0 wrappers still delegate to the numbered timer
   interfaces. */
static void test_sim_rtc_legacy_wrappers_use_timer_zero (void **state)
{
    RTC *rtc = &rtcs[0];

    (void)state;

    rtc->currd = 0;
    rtc->initd = 0;
    assert_int_equal (sim_rtc_init (200), 200);
    assert_int_equal (rtc->currd, 200);
    assert_int_equal (rtc->initd, 200);
    assert_int_equal (sim_rtc_calb (100), 200);
    assert_int_equal (sim_rtcn_calb_tick (0), 200);
}

/* Verify calibration ticks before the configured one-second boundary only
   count ticks and return the current calibrated delay. */
static void test_sim_rtcn_calb_counts_ticks_before_calibration (void **state)
{
    RTC *rtc = &rtcs[0];

    (void)state;

    assert_int_equal (sim_rtcn_calb (100, 0), 100);
    assert_int_equal (rtc->ticks, 1);
    assert_int_equal (rtc->elapsed, 0);
    assert_int_equal (rtc->calibrations, 0);
}

/* Verify the one-second calibration path updates elapsed time and keeps a
   stable current delay when wall time and virtual time agree. */
static void test_sim_rtcn_calb_updates_at_one_second_boundary (void **state)
{
    RTC *rtc = &rtcs[0];

    (void)state;

    rtc->ticks = 99;
    rtc->rtime = 1000;
    rtc->vtime = 1000;
    rtc->gtime = sim_gtime ();
    simh_test_clock_values[0] = (struct timespec){.tv_sec = 2,
                                                  .tv_nsec = 0};
    simh_test_clock_value_count = 1;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub, NULL);

    assert_int_equal (sim_rtcn_calb (100, 0), 100);
    assert_int_equal (rtc->ticks, 0);
    assert_int_equal (rtc->elapsed, 1);
    assert_int_equal (rtc->calibrations, 1);
    assert_int_equal (rtc->based, 100);
    assert_int_equal (rtc->currd, 100);
    assert_int_equal (rtc->nxintv, 1000);
}

/* Verify calibration treats a wrapping millisecond clock as running
   backwards and leaves the current delay unchanged. */
static void test_sim_rtcn_calb_handles_backwards_host_time (void **state)
{
    RTC *rtc = &rtcs[0];

    (void)state;

    rtc->ticks = 99;
    rtc->rtime = 1000;
    rtc->vtime = 1000;
    simh_test_clock_values[0] = (struct timespec){.tv_sec = 0,
                                                  .tv_nsec = 900000000L};
    simh_test_clock_value_count = 1;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub, NULL);

    assert_int_equal (sim_rtcn_calb (100, 0), 100);
    assert_int_equal (rtc->clock_calib_backwards, 1);
    assert_int_equal (rtc->rtime, 900);
    assert_int_equal (rtc->vtime, 900);
    assert_int_equal (rtc->based, 100);
    assert_int_equal (rtc->currd, 100);
}

/* Verify calibration ignores large host-clock gaps and restarts from the
   observed wall-clock time. */
static void test_sim_rtcn_calb_ignores_large_host_time_gap (void **state)
{
    RTC *rtc = &rtcs[0];

    (void)state;

    rtc->ticks = 99;
    rtc->rtime = 1000;
    rtc->vtime = 1000;
    simh_test_clock_values[0] = (struct timespec){.tv_sec = 32,
                                                  .tv_nsec = 0};
    simh_test_clock_value_count = 1;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub, NULL);

    assert_int_equal (sim_rtcn_calb (100, 0), 100);
    assert_int_equal (rtc->clock_calib_gap2big, 1);
    assert_int_equal (rtc->rtime, 32000);
    assert_int_equal (rtc->vtime, 32000);
    assert_int_equal (rtc->nxintv, 1000);
}

/* Verify init-all revisits only timers with saved initial delays and preserves
   their remembered clock units. */
static void test_sim_rtcn_init_all_reinitializes_saved_timers (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    rtcs[0].ticks = 25;
    rtcs[0].elapsed = 9;
    rtcs[0].calibrations = 4;
    rtcs[1].initd = 77;
    rtcs[1].currd = 0;

    sim_rtcn_init_all ();

    assert_ptr_equal (rtcs[0].clock_unit, &fixture->clock_unit);
    assert_int_equal (rtcs[0].ticks, 0);
    assert_int_equal (rtcs[0].elapsed, 0);
    assert_int_equal (rtcs[0].calibrations, 0);
    assert_int_equal (rtcs[1].currd, 77);
    assert_int_equal (rtcs[1].based, 77);
}

/* Verify timer service startup chooses the internal calibrated timer when no
   simulator clock is available. */
static void test_sim_start_timer_services_uses_internal_timer_without_clock (
    void **state)
{
    (void)state;

    sim_timer_init_with_deterministic_host_timing ();
    simh_test_clock_auto_step_msec = 0;
    sim_register_clock_unit_tmr (NULL, SIM_NTIMERS);

    sim_start_timer_services ();

    assert_int_equal (sim_rtcn_calibrated_tmr (), SIM_NTIMERS);
    assert_ptr_equal (rtcs[SIM_NTIMERS].clock_unit, &sim_internal_timer_unit);
    assert_true (sim_is_active (&sim_internal_timer_unit));
}

/* Verify stopping timer services migrates clock and coscheduled entries back
   to the standard event queue without leaving negative event catch-up. */
static void test_sim_stop_timer_services_migrates_clock_queues (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    assert_int_equal (sim_timer_activate_after (&fixture->clock_unit, 1000.0),
                      SCPE_OK);
    assert_int_equal (sim_clock_coschedule_tmr (&fixture->target_unit, 0, 3),
                      SCPE_OK);
    sim_interval = -5;

    sim_stop_timer_services ();

    assert_true (sim_interval >= 0);
    assert_false (sim_is_active (&sim_timer_units[0]));
    assert_true (sim_is_active (&fixture->clock_unit));
    assert_true (sim_is_active (&fixture->target_unit));
    assert_ptr_equal (rtcs[0].clock_cosched_queue, QUEUE_LIST_END);
    assert_null (fixture->target_unit.cancel);
    assert_true (fixture->target_unit.usecs_remaining >= 0.0);
}

/* Verify sim_os_ms_sleep uses the shared clock and nanosleep wrappers. */
static void test_sim_os_ms_sleep_uses_shared_time_wrappers (void **state)
{
    (void)state;

    simh_test_clock_values[0] = (struct timespec){.tv_sec = 1,
                                                  .tv_nsec = 100000000L};
    simh_test_clock_values[1] = (struct timespec){.tv_sec = 1,
                                                  .tv_nsec = 190000000L};
    simh_test_clock_value_count = 2;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub,
                             simh_test_nanosleep_stub);

    assert_int_equal (sim_os_ms_sleep (50), 90);
    assert_int_equal (simh_test_clock_calls, 2);
    assert_int_equal (simh_test_sleep_calls, 1);
    assert_int_equal (simh_test_last_sleep_req.tv_sec, 0);
    assert_int_equal (simh_test_last_sleep_req.tv_nsec, 50000000L);
}

/* Verify sim_timenow_double reads time through the shared clock wrapper. */
static void test_sim_timenow_double_uses_shared_clock_wrapper (void **state)
{
    double now;

    (void)state;

    simh_test_clock_values[0] = (struct timespec){.tv_sec = 42,
                                                  .tv_nsec = 125000000L};
    simh_test_clock_value_count = 1;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub, NULL);

    now = sim_timenow_double ();

    assert_int_equal (simh_test_clock_calls, 1);
    assert_int_equal (simh_test_last_clock_id, CLOCK_REALTIME);
    assert_float_equal (now, 42.125, 0.000001);
}

/* Verify the public time query wrappers use the shared clock hook and return
   seconds through both the return value and optional output pointer. */
static void test_sim_rtcn_get_time_and_sim_get_time_use_clock_wrapper (
    void **state)
{
    struct timespec now = {0};
    time_t seconds = 0;

    (void)state;

    simh_test_clock_values[0] = (struct timespec){.tv_sec = 123,
                                                  .tv_nsec = 456000000L};
    simh_test_clock_values[1] = (struct timespec){.tv_sec = 124,
                                                  .tv_nsec = 0};
    simh_test_clock_value_count = 2;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub, NULL);

    sim_rtcn_get_time (&now, 0);
    assert_int_equal (now.tv_sec, 123);
    assert_int_equal (now.tv_nsec, 456000000L);
    assert_int_equal (sim_get_time (&seconds), 124);
    assert_int_equal (seconds, 124);
    assert_int_equal (simh_test_clock_calls, 2);
    assert_int_equal (simh_test_last_clock_id, CLOCK_REALTIME);
}

/* Verify ROM delay reads tolerate high-bit input words without changing the
   caller-visible value. */
static void test_sim_rom_read_with_delay_accepts_high_bit_words (void **state)
{
    const uint32_t values[] = {
        0x80000000u,
        0x89abcdefu,
        0xffffffffu,
    };
    size_t i;

    (void)state;

    sim_set_rom_delay_factor (1);
    for (i = 0; i < sizeof (values) / sizeof (values[0]); ++i)
        assert_int_equal (sim_rom_read_with_delay (values[i]), values[i]);
}

/* Verify a deferred wall-clock activation tolerates a calibrated timer whose
   assist unit is not currently queued. */
static void test_sim_timer_activate_after_handles_inactive_calibrated_tick (
    void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    assert_false (sim_is_active (&fixture->clock_unit));

    assert_int_equal (
        sim_timer_activate_after (&fixture->target_unit, 1000.0), SCPE_OK);

    assert_true (sim_is_active (&fixture->target_unit));
    assert_float_equal (fixture->target_unit.usecs_remaining, 1000.0,
                        0.000001);
}

/* Verify long wall-clock activations are bounded before converting the
   instruction delay to the event queue's int32_t representation. */
static void test_sim_timer_activate_after_bounds_long_running_delay (
    void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    sim_is_running = true;

    assert_int_equal (
        sim_timer_activate_after (&fixture->target_unit, 30000000000000.0),
        SCPE_OK);

    assert_true (sim_is_active (&fixture->target_unit));
    assert_int_equal (fixture->target_unit.time, 0x7fffffff);
    assert_int_equal (sim_interval, 0x7fffffff);
}

/* Verify small non-zero wall-clock activations still get one event-queue
   instruction so they cannot silently become inactive. */
static void test_sim_timer_activate_after_preserves_minimum_delay (
    void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    assert_int_equal (sim_timer_activate_after (&fixture->target_unit, 1.0),
                      SCPE_OK);

    assert_true (sim_is_active (&fixture->target_unit));
    assert_int_equal (fixture->target_unit.time, 1);
    assert_int_equal (sim_interval, 1);
}

/* Verify activation requests for already queued units leave the existing queue
   entry unchanged. */
static void test_sim_timer_activate_after_preserves_active_unit (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    sim_is_running = true;
    assert_int_equal (sim_timer_activate_after (&fixture->target_unit, 1000.0),
                      SCPE_OK);
    assert_int_equal (sim_timer_activate_after (&fixture->target_unit, 5000.0),
                      SCPE_OK);

    assert_true (sim_is_active (&fixture->target_unit));
    assert_int_equal (fixture->target_unit.time, 10);
    assert_float_equal (fixture->target_unit.usecs_remaining, 0.0, 0.000001);
}

/* Verify negative wall-clock delays are loud internal errors, not overdue
   queue entries. */
static void test_sim_timer_activate_after_rejects_negative_delay (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;
    struct sim_timer_negative_delay_context context = {fixture, SCPE_OK};
    FILE *debug_stream;
    FILE *saved_deb = sim_deb;
    int32_t saved_deb_switches = sim_deb_switches;
    uint32_t saved_dctrl = sim_timer_dev.dctrl;
    char *debug_text;
    char *text = NULL;
    size_t text_size = 0;

    sim_is_running = true;
    sim_interval = 1234;
    debug_stream = tmpfile ();
    assert_non_null (debug_stream);
    sim_deb = debug_stream;
    sim_deb_switches |= SWMASK ('F');
    sim_timer_dev.dctrl |= TIMER_DBG_QUEUE;

    assert_int_equal (simh_test_capture_stdout (sim_timer_try_negative_activation,
                                                &context, &text, &text_size),
                      0);
    debug_text = sim_timer_read_stream_text (debug_stream);
    sim_deb = saved_deb;
    sim_deb_switches = saved_deb_switches;
    sim_timer_dev.dctrl = saved_dctrl;

    assert_int_equal (SCPE_BARE_STATUS (context.status), SCPE_IERR);
    assert_non_null (strstr (text, "Negative timer activation delay"));
    assert_non_null (strstr (text, "TMR0"));
    assert_non_null (strstr (debug_text, "negative delay"));
    assert_false (sim_is_active (&fixture->target_unit));
    assert_ptr_equal (sim_clock_queue, QUEUE_LIST_END);
    assert_int_equal (sim_interval, 1234);

    fclose (debug_stream);
    free (debug_text);
    free (text);
}

/* Verify the public double activation wrapper propagates negative-delay
   contract violations from the timer implementation. */
static void test_sim_activate_after_d_rejects_negative_delay (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;
    struct sim_timer_negative_delay_context context = {fixture, SCPE_OK};
    char *text = NULL;
    size_t text_size = 0;

    sim_is_running = true;
    sim_interval = 4321;

    assert_int_equal (
        simh_test_capture_stdout (sim_timer_try_negative_public_activation,
                                  &context, &text, &text_size),
        0);

    assert_int_equal (SCPE_BARE_STATUS (context.status), SCPE_IERR);
    assert_non_null (strstr (text, "Negative timer activation delay"));
    assert_false (sim_is_active (&fixture->target_unit));
    assert_ptr_equal (sim_clock_queue, QUEUE_LIST_END);
    assert_int_equal (sim_interval, 4321);

    free (text);
}

/* Verify the absolute public double wrapper rejects negative delays before
   canceling an existing activation. */
static void test_sim_activate_after_abs_d_rejects_negative_delay_before_cancel (
    void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;
    struct sim_timer_negative_delay_context context = {fixture, SCPE_OK};
    char *text = NULL;
    size_t text_size = 0;

    sim_is_running = true;
    assert_int_equal (sim_timer_activate_after (&fixture->target_unit, 1000.0),
                      SCPE_OK);

    assert_int_equal (
        simh_test_capture_stdout (sim_timer_try_negative_abs_public_activation,
                                  &context, &text, &text_size),
        0);

    assert_int_equal (SCPE_BARE_STATUS (context.status), SCPE_IERR);
    assert_non_null (strstr (text, "Negative timer activation delay"));
    assert_true (sim_is_active (&fixture->target_unit));
    assert_int_equal (fixture->target_unit.time, 10);
    assert_int_equal (sim_interval, 10);

    free (text);
}

/* Verify delays beyond the next calibration are coscheduled for the
   calibration tick and retain only the remaining wall-clock time. */
static void test_sim_timer_activate_after_coschedules_until_calibration (
    void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    sim_is_running = true;
    sim_timer_queue_calibrated_tick (fixture, 1000.0);

    assert_int_equal (
        sim_timer_activate_after (&fixture->target_unit, 1000000.0), SCPE_OK);

    assert_ptr_equal (rtcs[0].clock_cosched_queue, &fixture->target_unit);
    assert_int_equal (rtcs[0].cosched_interval, 99);
    assert_int_equal (fixture->target_unit.time, 99);
    assert_float_equal (fixture->target_unit.usecs_remaining, 9000.0,
                        0.000001);
}

/* Verify delays that fit before calibration but span more than one tick are
   coscheduled at the next clock tick with the remnant preserved. */
static void test_sim_timer_activate_after_coschedules_until_next_tick (
    void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    sim_is_running = true;
    sim_timer_queue_calibrated_tick (fixture, 1000.0);

    assert_int_equal (
        sim_timer_activate_after (&fixture->target_unit, 50000.0), SCPE_OK);

    assert_ptr_equal (rtcs[0].clock_cosched_queue, &fixture->target_unit);
    assert_int_equal (rtcs[0].cosched_interval, 0);
    assert_int_equal (fixture->target_unit.time, 0);
    assert_float_equal (fixture->target_unit.usecs_remaining, 49000.0,
                        0.000001);
}

/* Verify a clock unit activation schedules the timer assist unit and can be
   queried and canceled through the clock-unit wrapper API. */
static void test_sim_timer_clock_unit_activation_uses_assist_unit (
    void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    assert_int_equal (sim_timer_activate_after (&fixture->clock_unit, 1000.0),
                      SCPE_OK);

    assert_true (sim_is_active (&fixture->clock_unit));
    assert_true (sim_is_active (&sim_timer_units[0]));
    assert_true (sim_timer_is_active (&fixture->clock_unit));
    assert_float_equal (sim_activate_time_usecs (&fixture->clock_unit),
                        1000.0, 0.000001);
    assert_int_equal (sim_timer_cancel (&fixture->clock_unit), SCPE_OK);
    assert_false (sim_timer_is_active (&fixture->clock_unit));
}

/* Verify sim_cancel delegates registered clock-unit cancellation to the timer
   assist unit and reports success. */
static void test_sim_cancel_delegates_registered_clock_unit (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    assert_int_equal (sim_timer_activate_after (&fixture->clock_unit, 1000.0),
                      SCPE_OK);
    assert_true (sim_is_active (&fixture->clock_unit));
    assert_true (sim_is_active (&sim_timer_units[0]));

    assert_int_equal (sim_cancel (&fixture->clock_unit), SCPE_OK);

    assert_false (sim_timer_is_active (&fixture->clock_unit));
    assert_false (sim_is_active (&sim_timer_units[0]));
    assert_ptr_equal (sim_clock_queue, QUEUE_LIST_END);
}

/* Verify a stale timer-unit marker is reported instead of being silently
   treated as a normal inactive unit. */
static void test_sim_cancel_reports_stale_timer_unit_marker (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;
    UNIT stale_timer_unit = {0};

    stale_timer_unit.dptr = &fixture->target_device;
    stale_timer_unit.dynflags = UNIT_TMR_UNIT;
    assert_int_equal (sim_activate (&fixture->target_unit, 100), SCPE_OK);

    assert_int_equal (sim_cancel (&stale_timer_unit), SCPE_IERR);

    assert_true (sim_is_active (&fixture->target_unit));
    assert_int_equal (sim_activate_time (&fixture->target_unit), 101);
    assert_int_equal (sim_interval, 100);
}

/* Verify direct activation time helpers report inactive units and active
   standard queue units consistently. */
static void test_sim_timer_activation_time_queries_standard_queue (
    void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    assert_int_equal (_sim_timer_activate_time (&fixture->target_unit), -1);
    assert_float_equal (sim_timer_activate_time_usecs (&fixture->target_unit),
                        -1.0, 0.000001);

    sim_is_running = true;
    assert_int_equal (sim_timer_activate_after (&fixture->target_unit, 1000.0),
                      SCPE_OK);

    assert_int_equal (sim_activate_time (&fixture->target_unit), 11);
    assert_float_equal (sim_activate_time_usecs (&fixture->target_unit),
                        1000.0, 0.000001);
}

/* Verify coscheduled clock events are stored as delta ticks and preserve
   ordering when inserted at the head, middle, and tail. */
static void test_sim_clock_coschedule_orders_delta_tick_queue (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;
    UNIT first = {0};
    UNIT second = {0};
    UNIT third = {0};

    first.dptr = &fixture->target_device;
    second.dptr = &fixture->target_device;
    third.dptr = &fixture->target_device;

    assert_int_equal (sim_clock_coschedule_tmr (&second, 0, 5), SCPE_OK);
    assert_int_equal (sim_clock_coschedule_tmr (&first, 0, 2), SCPE_OK);
    assert_int_equal (sim_clock_coschedule_tmr (&third, 0, 8), SCPE_OK);

    assert_ptr_equal (rtcs[0].clock_cosched_queue, &first);
    assert_ptr_equal (first.next, &second);
    assert_ptr_equal (second.next, &third);
    assert_ptr_equal (third.next, QUEUE_LIST_END);
    assert_int_equal (rtcs[0].cosched_interval, 2);
    assert_int_equal (first.time, 2);
    assert_int_equal (second.time, 3);
    assert_int_equal (third.time, 3);

    assert_int_equal (sim_cancel (&second), SCPE_OK);
    assert_ptr_equal (first.next, &third);
    assert_int_equal (third.time, 6);
    assert_int_equal (sim_cancel (&third), SCPE_OK);
    assert_int_equal (sim_cancel (&first), SCPE_OK);
    assert_ptr_equal (rtcs[0].clock_cosched_queue, QUEUE_LIST_END);
}

/* Verify coscheduling rejects negative ticks and falls back to the standard
   queue for invalid timer numbers. */
static void test_sim_clock_coschedule_handles_invalid_requests (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    assert_int_equal (
        sim_clock_coschedule_tmr (&fixture->target_unit, 0, -1), SCPE_ARG);

    assert_int_equal (sim_clock_coschedule_tmr (&fixture->target_unit,
                                               SIM_NTIMERS + 1, 2),
                      SCPE_OK);
    assert_true (sim_is_active (&fixture->target_unit));
    assert_int_equal (sim_activate_time (&fixture->target_unit), 20001);
}

/* Verify the rounded public coschedule wrappers use the calibrated timer and
   the absolute forms replace an existing coscheduled request. */
static void test_sim_clock_coschedule_wrappers_round_to_ticks (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    assert_int_equal (sim_clock_coschedule (&fixture->target_unit, 249),
                      SCPE_OK);
    assert_ptr_equal (rtcs[0].clock_cosched_queue, &fixture->target_unit);
    assert_int_equal (fixture->target_unit.time, 2);

    assert_int_equal (sim_clock_coschedule_abs (&fixture->target_unit, 351),
                      SCPE_OK);
    assert_ptr_equal (rtcs[0].clock_cosched_queue, &fixture->target_unit);
    assert_int_equal (fixture->target_unit.time, 4);
    assert_int_equal (
        sim_clock_coschedule_tmr_abs (&fixture->target_unit, 0, 1), SCPE_OK);
    assert_int_equal (fixture->target_unit.time, 1);
    assert_int_equal (sim_cancel (&fixture->target_unit), SCPE_OK);
}

/* Verify clock tick acknowledgements enable catch-up tracking and reject
   invalid timer numbers without touching timer state. */
static void test_sim_rtcn_tick_ack_enables_catchup_tracking (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;
    RTC *rtc = &rtcs[0];

    assert_int_equal (sim_rtcn_tick_ack (5, -1), SCPE_TIMER);
    assert_false (rtc->clock_catchup_eligible);

    rtc->clock_tick_size = 0.01;
    simh_test_clock_values[0] = (struct timespec){.tv_sec = 10,
                                                  .tv_nsec = 0};
    simh_test_clock_values[1] = (struct timespec){.tv_sec = 10,
                                                  .tv_nsec = 20000000L};
    simh_test_clock_value_count = 2;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub, NULL);

    assert_int_equal (sim_rtcn_tick_ack (5, 0), SCPE_OK);
    assert_true (rtc->clock_catchup_eligible);
    assert_true (rtc->clock_catchup_pending);
    assert_int_equal (rtc->calib_ticks_acked, 1);
    assert_true (sim_is_active (&sim_timer_units[0]));
    assert_int_equal (sim_activate_time (&sim_timer_units[0]), 6);
    assert_ptr_equal (rtc->clock_unit, &fixture->clock_unit);
}

/* Verify a catch-up calibration tick is counted and clears the pending
   catch-up marker before the next tick is considered. */
static void test_sim_rtcn_calb_counts_pending_catchup_tick (void **state)
{
    RTC *rtc = &rtcs[0];

    (void)state;

    rtc->clock_catchup_pending = true;
    rtc->clock_tick_size = 0.01;

    assert_int_equal (sim_rtcn_calb (100, 0), 100);
    assert_false (rtc->clock_catchup_pending);
    assert_int_equal (rtc->clock_catchup_ticks, 1);
    assert_int_equal (rtc->clock_catchup_ticks_curr, 1);
}

/* Verify the timer assist service dispatches the registered clock and queues
   coscheduled work whose tick has arrived. */
static void test_sim_timer_tick_svc_dispatches_coscheduled_work (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    assert_int_equal (sim_clock_coschedule_tmr (&fixture->target_unit, 0, 0),
                      SCPE_OK);

    assert_int_equal (sim_timer_units[0].action (&sim_timer_units[0]),
                      SCPE_OK);

    assert_int_equal (simh_test_clock_service_calls, 1);
    assert_true (sim_is_active (&fixture->target_unit));
    assert_null (fixture->target_unit.cancel);

    assert_int_equal (sim_process_event (), SCPE_OK);
    assert_int_equal (simh_test_target_service_calls, 1);
}

/* Verify the timer assist service reports an internal error for a registered
   clock unit without a service routine. */
static void test_sim_timer_tick_svc_rejects_missing_clock_action (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    fixture->clock_unit.action = NULL;

    assert_int_equal (sim_timer_units[0].action (&sim_timer_units[0]),
                      SCPE_IERR);
}

/* Verify the clock queue show helper reports coscheduled entries. */
static void test_sim_show_clock_queues_reports_coscheduled_entries (
    void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;
    char *text;
    FILE *stream;

    assert_int_equal (sim_clock_coschedule_tmr (&fixture->target_unit, 0, 3),
                      SCPE_OK);

    stream = tmpfile ();
    assert_non_null (stream);
    assert_int_equal (
        sim_show_clock_queues (stream, NULL, NULL, 0, NULL), SCPE_OK);
    text = sim_timer_read_stream_text (stream);

    assert_non_null (strstr (text, "co-schedule event queue status"));
    assert_non_null (strstr (text, "on next tick"));

    free (text);
    fclose (stream);
    assert_int_equal (sim_cancel (&fixture->target_unit), SCPE_OK);
}

/* Verify idling exits safely when no calibrated timer exists and accounts for
   the instruction cycle already consumed by the caller. */
static void test_sim_idle_without_calibrated_timer_counts_single_cycle (
    void **state)
{
    (void)state;

    sim_interval = 100;
    sim_idle_enab = false;
    rtcs[0].elapsed = SIM_IDLE_STDFLT;

    assert_false (sim_idle (0, 7));
    assert_int_equal (sim_interval, 93);
}

/* Verify idling rejects out-of-range timer numbers before indexing the timer
   state array. */
static void test_sim_idle_rejects_invalid_timer_number (void **state)
{
    (void)state;

    sim_interval = 100;
    sim_idle_enab = true;

    assert_false (sim_idle (SIM_NTIMERS + 1, 7));
    assert_int_equal (sim_interval, 93);
}

/* Verify idling performs a deterministic sleep when the timer is stable and
   the pending queue entry is marked idle-capable. */
static void test_sim_idle_sleeps_for_idle_capable_event (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    simh_test_clock_auto_step_msec = 1;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub,
                             simh_test_nanosleep_stub);
    assert_int_equal (sim_timer_init (), 0);
    assert_int_equal (
        sim_rtcn_init_unit_ticks (&fixture->clock_unit, 100, 0, 100), 100);
    simh_test_clock_calls = 0;
    simh_test_sleep_calls = 0;
    simh_test_clock_auto_step_msec = 0;
    simh_test_clock_values[0] = (struct timespec){.tv_sec = 10,
                                                  .tv_nsec = 0};
    simh_test_clock_values[1] = (struct timespec){.tv_sec = 10,
                                                  .tv_nsec = 100000000L};
    simh_test_clock_value_count = 2;
    simh_test_clock_value_index = 0;
    rtcs[0].elapsed = SIM_IDLE_STDFLT;
    sim_idle_enab = true;
    fixture->target_unit.flags = UNIT_IDLE;
    assert_int_equal (sim_activate (&fixture->target_unit, 1000), SCPE_OK);

    assert_true (sim_idle (0, 0));
    assert_int_equal (simh_test_sleep_calls, 1);
    assert_int_equal (simh_test_last_sleep_req.tv_sec, 0);
    assert_int_equal (simh_test_last_sleep_req.tv_nsec, 100000000L);
    assert_int_equal (rtcs[0].clock_time_idled, 100);
    assert_true (sim_interval < 1000);
}

/* Verify an uncalibrated requested timer falls back to the calibrated timer
   for idle accounting. */
static void test_sim_idle_falls_back_to_calibrated_timer (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    rtcs[0].elapsed = SIM_IDLE_STDFLT;
    sim_idle_enab = true;
    fixture->target_unit.flags = UNIT_IDLE;
    assert_int_equal (sim_activate (&fixture->target_unit, 1000), SCPE_OK);
    simh_test_clock_values[0] = (struct timespec){.tv_sec = 10,
                                                  .tv_nsec = 0};
    simh_test_clock_values[1] = (struct timespec){.tv_sec = 10,
                                                  .tv_nsec = 100000000L};
    simh_test_clock_value_count = 2;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub,
                             simh_test_nanosleep_stub);

    assert_true (sim_idle (1, 0));

    assert_int_equal (simh_test_sleep_calls, 1);
    assert_int_equal (rtcs[0].clock_time_idled, 100);
    assert_int_equal (rtcs[1].clock_time_idled, 0);
}

/* Verify a pending catch-up tick prevents idling and queues the clock assist
   unit to run immediately. */
static void test_sim_idle_accelerates_pending_catchup_tick (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    sim_interval = 100;
    sim_idle_enab = true;
    rtcs[0].elapsed = SIM_IDLE_STDFLT;
    rtcs[0].clock_catchup_pending = true;
    fixture->target_unit.flags = UNIT_IDLE;
    assert_int_equal (sim_activate (&fixture->target_unit, 100), SCPE_OK);

    assert_false (sim_idle (0, 7));

    assert_true (sim_is_active (&sim_timer_units[0]));
    assert_int_equal (sim_activate_time (&sim_timer_units[0]), 1);
    assert_int_equal (sim_interval, -7);
}

/* Verify catch-up detection prevents idling and schedules an overdue clock
   assist tick. */
static void test_sim_idle_schedules_overdue_catchup_tick (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    sim_interval = 100;
    sim_idle_enab = true;
    rtcs[0].elapsed = SIM_IDLE_STDFLT;
    rtcs[0].clock_catchup_eligible = true;
    rtcs[0].clock_catchup_base_time = 10.0;
    rtcs[0].clock_tick_size = 0.01;
    simh_test_clock_values[0] = (struct timespec){.tv_sec = 10,
                                                  .tv_nsec = 20000000L};
    simh_test_clock_value_count = 1;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub, NULL);
    fixture->target_unit.flags = UNIT_IDLE;
    assert_int_equal (sim_activate (&fixture->target_unit, 100), SCPE_OK);

    assert_false (sim_idle (0, 7));

    assert_true (rtcs[0].clock_catchup_pending);
    assert_true (sim_is_active (&sim_timer_units[0]));
    assert_int_equal (sim_activate_time (&sim_timer_units[0]), 1);
    assert_int_equal (sim_interval, -7);
}

/* Verify idling refuses waits shorter than the useful minimum without calling
   the host sleep hook. */
static void test_sim_idle_rejects_too_short_wait (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    sim_interval = 1;
    sim_idle_enab = true;
    rtcs[0].elapsed = SIM_IDLE_STDFLT;
    fixture->target_unit.flags = UNIT_IDLE;
    assert_int_equal (sim_activate (&fixture->target_unit, 1), SCPE_OK);

    assert_false (sim_idle (0, 1));

    assert_int_equal (simh_test_sleep_calls, 0);
    assert_int_equal (sim_interval, 0);
}

/* Verify idling fails after setup if the calibrated timer cannot produce a
   cycles-per-millisecond value. */
static void test_sim_idle_rejects_timer_without_cycle_rate (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    sim_interval = 1000;
    sim_idle_enab = true;
    rtcs[0].currd = 1;
    rtcs[0].elapsed = SIM_IDLE_STDFLT;
    fixture->target_unit.flags = UNIT_IDLE;
    assert_int_equal (sim_activate (&fixture->target_unit, 1000), SCPE_OK);

    assert_false (sim_idle (0, 7));

    assert_int_equal (simh_test_sleep_calls, 0);
    assert_int_equal (sim_interval, 993);
}

/* Verify catch-up eligible idling uses the pending fraction of a clock tick
   when deciding whether to sleep. */
static void test_sim_idle_uses_catchup_tick_fraction_for_wait (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    sim_idle_enab = true;
    rtcs[0].elapsed = SIM_IDLE_STDFLT;
    rtcs[0].clock_catchup_eligible = true;
    rtcs[0].clock_catchup_base_time = 1000.0;
    rtcs[0].clock_tick_size = 0.01;
    fixture->target_unit.flags = UNIT_IDLE;
    assert_int_equal (sim_activate (&fixture->target_unit, 100), SCPE_OK);
    simh_test_clock_auto_step_msec = 100;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub,
                             simh_test_nanosleep_stub);

    assert_true (sim_idle (0, 0));

    assert_int_equal (simh_test_sleep_calls, 1);
    assert_int_equal (simh_test_last_sleep_req.tv_sec, 0);
    assert_int_equal (simh_test_last_sleep_req.tv_nsec, 10000000L);
}

/* Verify the user-facing idle setters accept the documented range and reject
   values below the minimum stability threshold. */
static void test_sim_set_idle_validates_stability_range (void **state)
{
    (void)state;

    assert_int_equal (SCPE_BARE_STATUS (sim_set_idle (NULL, 0, "1", NULL)),
                      SCPE_ARG);
    assert_false (sim_idle_enab);
    assert_int_equal (sim_set_idle (NULL, 0, "2", NULL), SCPE_OK);
    assert_true (sim_idle_enab);
    assert_int_equal (sim_clr_idle (NULL, 0, NULL, NULL), SCPE_OK);
    assert_false (sim_idle_enab);
}

/* Verify throttle setup validates availability and arguments before accepting
   the documented modes. */
static void test_sim_set_throt_validates_and_reports_modes (void **state)
{
    (void)state;

    assert_int_equal (SCPE_BARE_STATUS (sim_set_throt (1, "1M")),
                      SCPE_NOFNC);

    sim_timer_init_with_deterministic_host_timing ();
    simh_test_clock_auto_step_msec = 0;

    assert_int_equal (SCPE_BARE_STATUS (sim_set_throt (1, "")), SCPE_ARG);
    assert_int_equal (SCPE_BARE_STATUS (sim_set_throt (1, "BOGUS")),
                      SCPE_ARG);

    assert_int_equal (sim_set_throt (1, "2M"), SCPE_OK);
    sim_timer_assert_show_throt_contains ("Throttle:                      2 mega");

    assert_int_equal (sim_set_throt (1, "3K"), SCPE_OK);
    sim_timer_assert_show_throt_contains ("Throttle:                      3 kilo");

    assert_int_equal (sim_set_throt (1, "25%"), SCPE_OK);
    sim_timer_assert_show_throt_contains ("Throttle:                      25%");

    assert_int_equal (sim_set_throt (1, "5/2"), SCPE_OK);
    sim_timer_assert_show_throt_contains ("Throttle:                      5/2");
    sim_timer_assert_show_throt_contains (
        "Throttling by sleeping for:    2 ms every 5");

    assert_int_equal (SCPE_BARE_STATUS (sim_set_throt (0, "extra")),
                      SCPE_ARG);
    assert_int_equal (sim_set_throt (0, NULL), SCPE_OK);
}

/* Verify throttle scheduling and cancellation use the internal throttle unit
   without requiring simulator-specific device state. */
static void test_sim_throt_sched_and_cancel_manage_internal_unit (void **state)
{
    (void)state;

    sim_timer_init_with_deterministic_host_timing ();
    simh_test_clock_auto_step_msec = 0;
    assert_int_equal (sim_set_throt (1, "5/2"), SCPE_OK);

    sim_throt_sched ();

    assert_true (sim_is_active (&sim_throttle_unit));
    assert_int_equal (sim_activate_time (&sim_throttle_unit), 6);

    sim_throt_cancel ();

    assert_false (sim_is_active (&sim_throttle_unit));
}

/* Verify the periodic throttle service sleeps for the configured interval and
   reschedules itself after the configured instruction wait. */
static void test_sim_throt_svc_specific_mode_sleeps_and_reschedules (
    void **state)
{
    (void)state;

    sim_timer_init_with_deterministic_host_timing ();
    assert_int_equal (sim_set_throt (1, "5/2"), SCPE_OK);
    simh_test_clock_calls = 0;
    simh_test_sleep_calls = 0;
    simh_test_clock_auto_step_msec = 1;

    assert_int_equal (sim_throt_svc (&sim_throttle_unit), SCPE_OK);

    assert_int_equal (simh_test_sleep_calls, 1);
    assert_true (sim_is_active (&sim_throttle_unit));
    assert_int_equal (sim_activate_time (&sim_throttle_unit), 6);
}

/* Verify dynamic throttle modes take their first measurement and schedule
   their timing pass using the requested instruction rate. */
static void test_sim_throt_svc_dynamic_modes_enter_timing_state (void **state)
{
    static const struct sim_timer_dynamic_throttle_case cases[] = {
        {"2M", SIM_THROT_MCYC, 2, 2000000},
        {"3K", SIM_THROT_KCYC, 3, 3000},
    };
    size_t i;

    (void)state;

    for (i = 0; i < sizeof (cases) / sizeof (cases[0]); ++i) {
        struct sim_timer_throttle_test_state throttle;

        sim_timer_reset_initialized_host_timing ();
        assert_int_equal (sim_set_throt (1, cases[i].spec), SCPE_OK);

        throttle = sim_timer_throttle_state ();
        assert_int_equal (throttle.type, cases[i].type);
        assert_int_equal (throttle.val, cases[i].val);
        assert_int_equal (throttle.state, SIM_THROT_STATE_INIT);
        assert_int_equal (throttle.sleep_time, 1);

        assert_int_equal (sim_throt_svc (&sim_throttle_unit), SCPE_OK);

        throttle = sim_timer_throttle_state ();
        assert_int_equal (throttle.state, SIM_THROT_STATE_TIME);
        assert_int_equal (throttle.wait, cases[i].wait);
        assert_int_equal (simh_test_sleep_calls, 1);
        assert_true (sim_is_active (&sim_throttle_unit));
        assert_int_equal (sim_activate_time (&sim_throttle_unit),
                          cases[i].wait + 1);
    }
}

/* Verify dynamic throttle calibration computes a stable sleep interval when
   enough host time and simulated instructions have elapsed. */
static void test_sim_throt_svc_dynamic_time_state_calibrates_wait (
    void **state)
{
    struct sim_timer_throttle_test_state throttle;

    (void)state;

    sim_timer_reset_initialized_host_timing ();
    assert_int_equal (sim_set_throt (1, "1M"), SCPE_OK);
    simh_test_clock_auto_step_msec = 1;
    assert_int_equal (sim_throt_svc (&sim_throttle_unit), SCPE_OK);
    simh_test_clock_auto_step_msec = 100;

    sim_timer_process_due_throttle_event (0);

    throttle = sim_timer_throttle_state ();
    assert_int_equal (throttle.type, SIM_THROT_MCYC);
    assert_int_equal (throttle.state, SIM_THROT_STATE_THROTTLE);
    assert_int_equal (throttle.sleep_time, 1);
    assert_int_equal (throttle.wait, 1111);
    assert_float_equal (throttle.cps, 1000000.0, 0.000001);
    assert_true (sim_is_active (&sim_throttle_unit));
    assert_int_equal (sim_activate_time (&sim_throttle_unit), 1112);
}

/* Verify a dynamic throttle timing sample that is too short extends the
   measurement interval instead of declaring a calibrated throttle. */
static void test_sim_throt_svc_dynamic_time_state_retries_short_sample (
    void **state)
{
    struct sim_timer_throttle_test_state throttle;

    (void)state;

    sim_timer_reset_initialized_host_timing ();
    assert_int_equal (sim_set_throt (1, "1M"), SCPE_OK);
    simh_test_clock_auto_step_msec = 1;
    assert_int_equal (sim_throt_svc (&sim_throttle_unit), SCPE_OK);
    simh_test_sleep_calls = 0;
    simh_test_clock_auto_step_msec = 1;

    sim_timer_process_due_throttle_event (0);

    throttle = sim_timer_throttle_state ();
    assert_int_equal (throttle.state, SIM_THROT_STATE_TIME);
    assert_int_equal (throttle.wait, 4000000);
    assert_int_equal (simh_test_sleep_calls, 1);
    assert_true (sim_is_active (&sim_throttle_unit));
    assert_int_equal (sim_activate_time (&sim_throttle_unit), 4000001);
}

/* Verify an impossibly short but very large instruction sample disables
   dynamic throttling as the current user-visible behavior specifies. */
static void test_sim_throt_svc_dynamic_time_state_disables_too_fast_cpu (
    void **state)
{
    struct sim_timer_throttle_test_state throttle;

    (void)state;

    sim_timer_reset_initialized_host_timing ();
    assert_int_equal (sim_set_throt (1, "200M"), SCPE_OK);
    simh_test_clock_auto_step_msec = 1;
    assert_int_equal (sim_throt_svc (&sim_throttle_unit), SCPE_OK);
    simh_test_clock_auto_step_msec = 1;

    sim_timer_process_due_throttle_event (0);

    throttle = sim_timer_throttle_state ();
    assert_int_equal (throttle.type, SIM_THROT_NONE);
    assert_int_equal (throttle.state, SIM_THROT_STATE_INIT);
    assert_false (sim_is_active (&sim_throttle_unit));
}

/* Verify dynamic throttling disables itself when the measured host rate is
   below the requested simulated rate and no higher peak rate is known. */
static void test_sim_throt_svc_dynamic_time_state_disables_slow_cpu (
    void **state)
{
    struct sim_timer_throttle_test_state throttle;

    (void)state;

    sim_timer_reset_initialized_host_timing ();
    assert_int_equal (sim_set_throt (1, "2M"), SCPE_OK);
    simh_test_clock_auto_step_msec = 1;
    assert_int_equal (sim_throt_svc (&sim_throttle_unit), SCPE_OK);
    simh_test_clock_auto_step_msec = 2000;

    sim_timer_process_due_throttle_event (0);

    throttle = sim_timer_throttle_state ();
    assert_int_equal (throttle.type, SIM_THROT_NONE);
    assert_int_equal (throttle.state, SIM_THROT_STATE_INIT);
    assert_false (sim_is_active (&sim_throttle_unit));
}

/* Verify calibrated startup defers dynamic throttling until the simulator
   clock is stable, then uses the clock peak rate to recover from a slow
   measurement sample. */
static void test_sim_throt_svc_calibrated_startup_uses_peak_rate (
    void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;
    struct sim_timer_throttle_test_state throttle;

    sim_timer_start_calibrated_dynamic_throttle ("48%", 5000);

    throttle = sim_timer_throttle_state ();
    assert_ptr_equal (rtcs[0].clock_unit, &fixture->clock_unit);
    assert_int_equal (throttle.state, SIM_THROT_STATE_THROTTLE);
    assert_float_equal (throttle.peak_cps, 10000.0, 0.000001);
    assert_float_equal (throttle.cps, 4800.0, 0.000001);
    assert_int_equal (throttle.sleep_time, 1);
    assert_int_equal (throttle.wait, 119);
    assert_int_equal (rtcs[0].currd, 48);
    assert_int_equal (rtcs[0].based, 48);
    assert_int_equal (rtcs[0].ticks, 99);
}

/* Verify dynamic throttling lengthens its wait when periodic recalibration
   observes execution slower than the desired rate. */
static void test_sim_throt_svc_dynamic_throttle_adjusts_slow_drift (
    void **state)
{
    struct sim_timer_throttle_test_state before;
    struct sim_timer_throttle_test_state after;

    (void)state;

    sim_timer_reset_initialized_host_timing ();
    assert_int_equal (sim_set_throt (1, "1M"), SCPE_OK);
    simh_test_clock_auto_step_msec = 1;
    assert_int_equal (sim_throt_svc (&sim_throttle_unit), SCPE_OK);
    simh_test_clock_auto_step_msec = 100;
    sim_timer_process_due_throttle_event (0);
    before = sim_timer_throttle_state ();
    assert_int_equal (before.state, SIM_THROT_STATE_THROTTLE);

    simh_test_clock_auto_step_msec = 10000;
    sim_timer_process_due_throttle_event (0);

    after = sim_timer_throttle_state ();
    assert_int_equal (after.state, SIM_THROT_STATE_THROTTLE);
    assert_int_equal (after.sleep_time, before.sleep_time);
    assert_true (after.wait > before.wait);
    assert_float_equal (after.cps, 1000000.0, 0.000001);
}

/* Verify dynamic throttling recomputes its wait from the known peak rate when
   periodic recalibration observes execution far above the desired rate. */
static void test_sim_throt_svc_dynamic_throttle_recovers_fast_drift (
    void **state)
{
    struct sim_timer_throttle_test_state before;
    struct sim_timer_throttle_test_state after;

    (void)state;

    sim_timer_start_calibrated_dynamic_throttle ("9K", 1000);
    before = sim_timer_throttle_state ();
    assert_int_equal (before.wait, 90);

    simh_test_clock_auto_step_msec = 10000;
    sim_timer_process_due_throttle_event (1000000);

    after = sim_timer_throttle_state ();
    assert_int_equal (after.state, SIM_THROT_STATE_THROTTLE);
    assert_float_equal (after.peak_cps, before.peak_cps, 0.000001);
    assert_float_equal (after.cps, 9000.0, 0.000001);
    assert_int_equal (after.sleep_time, before.sleep_time);
    assert_true (after.ms_start > before.ms_start);
    assert_true (after.wait >= SIM_THROT_WMIN);
}

/* Verify fixed-period throttling records the observed instruction rate during
   its periodic ten-second refresh without changing throttle mode. */
static void test_sim_throt_svc_specific_mode_records_periodic_rate (
    void **state)
{
    struct sim_timer_throttle_test_state throttle;

    (void)state;

    sim_timer_reset_initialized_host_timing ();
    assert_int_equal (sim_set_throt (1, "50000/2"), SCPE_OK);
    simh_test_clock_auto_step_msec = 1;
    sim_throt_sched ();
    simh_test_clock_auto_step_msec = 10000;

    sim_timer_process_due_throttle_event (0);

    throttle = sim_timer_throttle_state ();
    assert_int_equal (throttle.type, SIM_THROT_SPC);
    assert_int_equal (throttle.state, SIM_THROT_STATE_THROTTLE);
    assert_int_equal (throttle.sleep_time, 2);
    assert_int_equal (throttle.wait, 50000);
    assert_true (throttle.cps > 0.0);
}

/* Verify the detailed timer report includes calibrated clock, skip counters,
   catch-up timing, and throttling sections. */
static void test_sim_show_timers_reports_detailed_timer_state (void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;
    RTC *rtc = &rtcs[0];
    char *text;
    FILE *stream;

    rtc->elapsed = 3;
    rtc->calibrations = 2;
    rtc->clock_calib_skip_idle = 1;
    rtc->clock_calib_backwards = 1;
    rtc->clock_calib_gap2big = 1;
    rtc->gtime = 1234.0;
    rtc->clock_ticks = 4;
    rtc->clock_ticks_tot = 5;
    rtc->clock_skew_max = -0.25;
    rtc->calib_ticks_acked = 6;
    rtc->calib_ticks_acked_tot = 7;
    rtc->calib_tick_time = 0.5;
    rtc->calib_tick_time_tot = 1.0;
    rtc->clock_catchup_eligible = true;
    rtc->clock_catchup_base_time = 100.0;
    rtc->clock_catchup_ticks = 8;
    rtc->clock_catchup_ticks_curr = 1;
    rtc->clock_catchup_ticks_tot = 9;
    rtc->clock_time_idled = 250;
    assert_int_equal (sim_set_timers (0, "CALIB=50"), SCPE_OK);
    assert_int_equal (sim_set_idle (NULL, 0, "2", NULL), SCPE_OK);
    assert_true (sim_idle_enab);
    assert_int_equal (sim_set_throt (1, "5/2"), SCPE_OK);
    assert_false (sim_idle_enab);
    simh_test_clock_values[0] = (struct timespec){.tv_sec = 200,
                                                  .tv_nsec = 125000000L};
    simh_test_clock_value_count = 1;
    sim_time_set_test_hooks (simh_test_clock_gettime_stub, NULL);

    stream = tmpfile ();
    assert_non_null (stream);
    assert_int_equal (sim_show_timers (stream, NULL, &fixture->clock_unit, 0,
                                       NULL),
                      SCPE_OK);
    text = sim_timer_read_stream_text (stream);

    assert_non_null (strstr (text, "Throttle:                      5/2"));
    assert_non_null (strstr (text, "Calibrated Timer:               CLK0"));
    assert_non_null (
        strstr (text, "Calibration:                    Skipped when Idle"));
    assert_non_null (strstr (text, "zimh-unit-sim-timer clock device is CLK0"));
    assert_non_null (strstr (text, "Calibrated Timer 0:"));
    assert_non_null (strstr (text, "Calibs Skip While Idle"));
    assert_non_null (strstr (text, "Calibs Skip Backwards"));
    assert_non_null (strstr (text, "Calibs Skip Gap Too Big"));
    assert_non_null (strstr (text, "Peak Clock Skew"));
    assert_non_null (strstr (text, "Ticks Acked"));
    assert_non_null (strstr (text, "Catchup Tick Time"));
    assert_non_null (strstr (text, "Total Time Idled"));

    free (text);
    fclose (stream);
}

/* Verify the timer report names the internal calibrated timer fallback. */
static void test_sim_show_timers_reports_internal_timer_fallback (void **state)
{
    char *text;
    FILE *stream;

    (void)state;

    sim_timer_init_with_deterministic_host_timing ();
    simh_test_clock_auto_step_msec = 0;
    sim_register_clock_unit_tmr (NULL, SIM_NTIMERS);
    sim_start_timer_services ();

    stream = tmpfile ();
    assert_non_null (stream);
    assert_int_equal (sim_show_timers (stream, NULL, NULL, 0, NULL), SCPE_OK);
    text = sim_timer_read_stream_text (stream);

    assert_non_null (
        strstr (text, "Calibrated Timer:               Internal Timer"));
    assert_non_null (strstr (text, "Catchup Ticks:"));

    free (text);
    fclose (stream);
}

/* Verify the timer report describes enabled idling and always-on
   calibration. */
static void test_sim_show_timers_reports_idle_status (void **state)
{
    char *text;
    FILE *stream;

    (void)state;

    sim_timer_init_with_deterministic_host_timing ();
    simh_test_clock_auto_step_msec = 0;
    assert_int_equal (sim_set_timers (0, "CATCHUP,CALIB=ALWAYS"), SCPE_OK);
    assert_int_equal (sim_set_idle (NULL, 0, "3", NULL), SCPE_OK);

    stream = tmpfile ();
    assert_non_null (stream);
    assert_int_equal (sim_show_timers (stream, NULL, NULL, 0, NULL), SCPE_OK);
    text = sim_timer_read_stream_text (stream);

    assert_non_null (strstr (text, "Idling:                         Enabled"));
    assert_non_null (
        strstr (text, "Time before Idling starts:      3 seconds"));
    assert_non_null (strstr (text, "Calibration:                    Always"));

    free (text);
    fclose (stream);
}

/* Verify SET CLOCK STOP schedules the internal stop unit and rejects times
   that are not in the future. */
static void test_sim_set_timers_stop_schedules_stop_unit (void **state)
{
    int stop_time;
    int expected_delay;
    char stop_spec[32];

    (void)state;

    sim_timer_init_with_deterministic_host_timing ();
    simh_test_clock_auto_step_msec = 0;
    stop_time = (int)sim_gtime () + 100;
    expected_delay = stop_time - (int)sim_gtime ();

    assert_int_equal (SCPE_BARE_STATUS (sim_set_timers (0, "STOP=0")),
                      SCPE_ARG);
    assert_true (snprintf (stop_spec, sizeof (stop_spec), "STOP=%d",
                           stop_time) < (int)sizeof (stop_spec));
    assert_int_equal (sim_set_timers (0, stop_spec), SCPE_OK);
    assert_true (sim_is_active (&sim_stop_unit));
    assert_int_equal (sim_activate_time (&sim_stop_unit), expected_delay + 1);
    assert_int_equal (sim_stop_unit.action (&sim_stop_unit), SCPE_STOP);
}

/* Verify public clock registration handles invalid timer numbers and migrates
   coscheduled entries when a clock unit is deregistered. */
static void test_sim_register_clock_unit_migrates_coscheduled_work (
    void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    assert_int_equal (
        sim_register_clock_unit_tmr (&fixture->target_unit, -1), SCPE_IERR);
    assert_int_equal (sim_clock_coschedule_tmr (&fixture->target_unit, 0, 3),
                      SCPE_OK);

    assert_int_equal (sim_register_clock_unit_tmr (NULL, 0), SCPE_OK);

    assert_null (rtcs[0].clock_unit);
    assert_ptr_equal (rtcs[0].clock_cosched_queue, QUEUE_LIST_END);
    assert_false (fixture->clock_unit.dynflags & UNIT_TMR_UNIT);
    assert_true (sim_is_active (&fixture->target_unit));
    assert_null (fixture->target_unit.cancel);
    assert_true (fixture->target_unit.usecs_remaining > 0.0);
}

/* Verify the public timer activation wrapper converts instruction delay
   through the calibrated execution rate before queuing. */
static void test_sim_timer_activate_wrapper_converts_interval_to_usecs (
    void **state)
{
    struct sim_timer_activation_fixture *fixture = *state;

    sim_is_running = true;
    assert_int_equal (sim_timer_activate (&fixture->target_unit, 50), SCPE_OK);

    assert_true (sim_is_active (&fixture->target_unit));
    assert_int_equal (fixture->target_unit.time, 50);
    assert_float_equal (sim_timer_activate_time_usecs (&fixture->target_unit),
                        5000.0, 0.000001);
}

/* Verify idle and catchup show/set helpers emit their current user-facing
   status text and accept comma-separated SET CLOCK modifiers. */
static void test_sim_timer_set_and_show_helpers (void **state)
{
    char *text;
    FILE *stream;

    (void)state;

    assert_int_equal (sim_set_timers (0, NULL), SCPE_2FARG);
    assert_int_equal (sim_set_timers (0, "BOGUS"), SCPE_NOPARAM);
    assert_int_equal (SCPE_BARE_STATUS (sim_set_timers (0, "CALIB=0")),
                      SCPE_ARG);
    assert_int_equal (sim_set_timers (0, "NOCATCHUP,CALIB=50"), SCPE_OK);

    stream = tmpfile ();
    assert_non_null (stream);
    assert_int_equal (sim_timer_show_catchup (stream, NULL, 0, NULL),
                      SCPE_OK);
    text = sim_timer_read_stream_text (stream);
    assert_non_null (strstr (text, "Calibrated Ticks"));
    assert_null (strstr (text, "with Catchup Ticks"));
    free (text);
    fclose (stream);

    assert_int_equal (sim_set_timers (0, "CATCHUP,CALIB=ALWAYS"), SCPE_OK);
    stream = tmpfile ();
    assert_non_null (stream);
    assert_int_equal (sim_timer_show_catchup (stream, NULL, 0, NULL),
                      SCPE_OK);
    text = sim_timer_read_stream_text (stream);
    assert_non_null (strstr (text, "with Catchup Ticks"));
    free (text);
    fclose (stream);

    stream = tmpfile ();
    assert_non_null (stream);
    assert_int_equal (sim_show_idle (stream, NULL, 0, NULL), SCPE_OK);
    text = sim_timer_read_stream_text (stream);
    assert_non_null (strstr (text, "idle disabled"));
    free (text);
    fclose (stream);

    assert_int_equal (sim_set_idle (NULL, 0, "2", NULL), SCPE_OK);
    stream = tmpfile ();
    assert_non_null (stream);
    assert_int_equal (sim_show_idle (stream, NULL, 0, NULL), SCPE_OK);
    text = sim_timer_read_stream_text (stream);
    assert_non_null (strstr (text, "idle enabled"));
    free (text);
    fclose (stream);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_sim_timespec_diff_borrows_and_normalizes,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timespec_diff_handles_simple_difference,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_deadline_msec_builds_absolute_deadline,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_deadline_msec_reports_clock_failure,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_idle_capable_reports_default_state,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_init_measures_host_timing_with_hooks,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_default_query_helpers_are_stable,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_os_msec_uses_shared_clock_wrapper,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_os_msec_handles_epoch_scale_seconds,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_os_ms_sleep_uses_shared_time_wrappers,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timenow_double_uses_shared_clock_wrapper,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_rtcn_get_time_and_sim_get_time_use_clock_wrapper,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_rtcn_init_rejects_invalid_timer_numbers,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_rtcn_init_unit_ticks_records_clock_state,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_rtc_legacy_wrappers_use_timer_zero,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_rtcn_calb_counts_ticks_before_calibration,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_rtcn_calb_updates_at_one_second_boundary,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_rtcn_calb_handles_backwards_host_time,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_rtcn_calb_ignores_large_host_time_gap,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_rtcn_init_all_reinitializes_saved_timers,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_start_timer_services_uses_internal_timer_without_clock,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_stop_timer_services_migrates_clock_queues,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_rom_read_with_delay_accepts_high_bit_words,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_activate_after_handles_inactive_calibrated_tick,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_activate_after_bounds_long_running_delay,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_activate_after_preserves_minimum_delay,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_activate_after_preserves_active_unit,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_activate_after_rejects_negative_delay,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_activate_after_d_rejects_negative_delay,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_activate_after_abs_d_rejects_negative_delay_before_cancel,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_activate_after_coschedules_until_calibration,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_activate_after_coschedules_until_next_tick,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_clock_unit_activation_uses_assist_unit,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_cancel_delegates_registered_clock_unit,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_cancel_reports_stale_timer_unit_marker,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_activation_time_queries_standard_queue,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_clock_coschedule_orders_delta_tick_queue,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_clock_coschedule_handles_invalid_requests,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_clock_coschedule_wrappers_round_to_ticks,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_rtcn_tick_ack_enables_catchup_tracking,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_rtcn_calb_counts_pending_catchup_tick,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_tick_svc_dispatches_coscheduled_work,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_tick_svc_rejects_missing_clock_action,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_show_clock_queues_reports_coscheduled_entries,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_idle_without_calibrated_timer_counts_single_cycle,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_idle_rejects_invalid_timer_number,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_idle_sleeps_for_idle_capable_event,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_idle_falls_back_to_calibrated_timer,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_idle_accelerates_pending_catchup_tick,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_idle_schedules_overdue_catchup_tick,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_idle_rejects_too_short_wait,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_idle_rejects_timer_without_cycle_rate,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_idle_uses_catchup_tick_fraction_for_wait,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_idle_validates_stability_range,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_throt_validates_and_reports_modes,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_throt_sched_and_cancel_manage_internal_unit,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_throt_svc_specific_mode_sleeps_and_reschedules,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_throt_svc_dynamic_modes_enter_timing_state,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_throt_svc_dynamic_time_state_calibrates_wait,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_throt_svc_dynamic_time_state_retries_short_sample,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_throt_svc_dynamic_time_state_disables_too_fast_cpu,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_throt_svc_dynamic_time_state_disables_slow_cpu,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_throt_svc_calibrated_startup_uses_peak_rate,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_throt_svc_dynamic_throttle_adjusts_slow_drift,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_throt_svc_dynamic_throttle_recovers_fast_drift,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_throt_svc_specific_mode_records_periodic_rate,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_show_timers_reports_detailed_timer_state,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_show_timers_reports_internal_timer_fallback,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_show_timers_reports_idle_status,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_timers_stop_schedules_stop_unit,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_register_clock_unit_migrates_coscheduled_work,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_activate_wrapper_converts_interval_to_usecs,
            setup_sim_timer_activation_fixture,
            teardown_sim_timer_activation_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_timer_set_and_show_helpers,
            setup_sim_timer_fixture, teardown_sim_timer_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
