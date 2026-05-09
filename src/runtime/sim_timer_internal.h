/* sim_timer_internal.h: private timer library state */
// SPDX-FileCopyrightText: 1993-2022 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef SIM_TIMER_INTERNAL_H_
#define SIM_TIMER_INTERNAL_H_

#include <stdbool.h>
#include <stdio.h>

#include "sim_defs.h"

typedef struct RTC {
    UNIT *clock_unit; /* registered ticking clock unit */
    UNIT *timer_unit; /* related clock assist unit */
    UNIT *clock_cosched_queue;
    int32 cosched_interval;
    uint32 ticks;                 /* ticks */
    uint32 hz;                    /* tick rate */
    uint32 last_hz;               /* prior tick rate */
    uint32 rtime;                 /* real time (usecs) */
    uint32 vtime;                 /* virtual time (usecs) */
    double gtime;                 /* instruction time */
    uint32 nxintv;                /* next interval */
    int32 based;                  /* base delay */
    int32 currd;                  /* current delay */
    int32 initd;                  /* initial delay */
    uint32 elapsed;               /* seconds since init */
    uint32 calibrations;          /* calibration count */
    double clock_skew_max;        /* asynchronous max skew */
    double clock_tick_size;       /* 1/hz */
    uint32 calib_initializations; /* Initialization Count */
    double calib_tick_time;       /* ticks time */
    double calib_tick_time_tot;   /* ticks time - total */
    uint32 calib_ticks_acked;     /* ticks Acked */
    uint32 calib_ticks_acked_tot; /* ticks Acked - total */
    uint32 clock_ticks;           /* ticks delivered since catchup base */
    uint32 clock_ticks_tot;       /* clock ticks since catchup base - total */
    double clock_init_base_time;  /* reference time for clock initialization */
    double clock_tick_start_time; /* reference time when ticking started */
    double clock_catchup_base_time;  /* reference time for catchup ticks */
    uint32 clock_catchup_ticks;      /* Record of catchups */
    uint32 clock_catchup_ticks_tot;  /* Record of catchups - total */
    uint32 clock_catchup_ticks_curr; /* Record of catchups in this second */
    bool clock_catchup_pending;      /* clock tick catchup pending */
    bool clock_catchup_eligible;     /* clock tick catchup eligible */
    uint32 clock_time_idled;         /* total time idled */
    uint32 clock_time_idled_last;    /* total idled time before prior second */
    uint32 clock_calib_skip_idle;    /* calibrations skipped due to idling */
    uint32 clock_calib_gap2big;      /* calibrations skipped: gap too big */
    uint32 clock_calib_backwards;    /* calibrations skipped: clock backwards */
} RTC;

/* Test-only snapshot of private throttle state-machine internals. */
struct sim_timer_throttle_test_state {
    uint32 ms_start;
    uint32 ms_stop;
    uint32 type;
    uint32 val;
    uint32 state;
    double cps;
    double peak_cps;
    double inst_start;
    uint32 sleep_time;
    int32 wait;
};

extern RTC rtcs[SIM_NTIMERS + 1];
extern UNIT sim_timer_units[SIM_NTIMERS + 1];
extern UNIT sim_stop_unit;
extern UNIT sim_internal_timer_unit;
extern UNIT sim_throttle_unit;
extern int32 sim_internal_timer_time;

/* Restore timer globals to startup-like defaults for isolated unit tests.
   Production code should use the normal timer lifecycle APIs instead. */
void sim_timer_reset_test_state(void);

/* Snapshot throttle internals for deterministic state-machine tests. */
void sim_timer_get_throttle_test_state(
    struct sim_timer_throttle_test_state *state);

t_stat sim_throt_svc(UNIT *uptr);
t_stat sim_timer_show_catchup(FILE *st, UNIT *uptr, int32 val,
                              const void *desc);

#endif
