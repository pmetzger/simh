/* sim_timer_internal.h: private timer library state */
// SPDX-FileCopyrightText: 1993-2022 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef SIM_TIMER_INTERNAL_H_
#define SIM_TIMER_INTERNAL_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "sim_defs.h"

typedef struct RTC {
    UNIT *clock_unit; /* registered ticking clock unit */
    UNIT *timer_unit; /* related clock assist unit */
    UNIT *clock_cosched_queue;
    int32_t cosched_interval;
    uint32_t ticks;               /* ticks */
    uint32_t hz;                  /* tick rate */
    uint32_t last_hz;             /* prior tick rate */
    uint32_t rtime;               /* real time (usecs) */
    uint32_t vtime;               /* virtual time (usecs) */
    double gtime;                 /* instruction time */
    uint32_t nxintv;              /* next interval */
    int32_t based;                /* base delay */
    int32_t currd;                /* current delay */
    int32_t initd;                /* initial delay */
    uint32_t elapsed;             /* seconds since init */
    uint32_t calibrations;        /* calibration count */
    double clock_skew_max;        /* asynchronous max skew */
    double clock_tick_size;       /* 1/hz */
    uint32_t calib_initializations; /* Initialization Count */
    double calib_tick_time;       /* ticks time */
    double calib_tick_time_tot;   /* ticks time - total */
    uint32_t calib_ticks_acked;   /* ticks Acked */
    uint32_t calib_ticks_acked_tot; /* ticks Acked - total */
    uint32_t clock_ticks;         /* ticks delivered since catchup base */
    uint32_t clock_ticks_tot;     /* clock ticks since catchup base - total */
    double clock_init_base_time;  /* reference time for clock initialization */
    double clock_tick_start_time; /* reference time when ticking started */
    double clock_catchup_base_time;  /* reference time for catchup ticks */
    uint32_t clock_catchup_ticks;    /* Record of catchups */
    uint32_t clock_catchup_ticks_tot; /* Record of catchups - total */
    uint32_t clock_catchup_ticks_curr; /* Record of catchups in this second */
    bool clock_catchup_pending;      /* clock tick catchup pending */
    bool clock_catchup_eligible;     /* clock tick catchup eligible */
    uint32_t clock_time_idled;       /* total time idled */
    uint32_t clock_time_idled_last;  /* total idled time before prior second */
    uint32_t clock_calib_skip_idle;  /* calibrations skipped due to idling */
    uint32_t clock_calib_gap2big;    /* calibrations skipped: gap too big */
    uint32_t clock_calib_backwards;  /* calibrations skipped: clock backwards */
} RTC;

/* Test-only snapshot of private throttle state-machine internals. */
struct sim_timer_throttle_test_state {
    uint32_t ms_start;
    uint32_t ms_stop;
    uint32_t type;
    uint32_t val;
    uint32_t state;
    double cps;
    double peak_cps;
    double inst_start;
    uint32_t sleep_time;
    int32_t wait;
};

extern RTC rtcs[SIM_NTIMERS + 1];
extern UNIT sim_timer_units[SIM_NTIMERS + 1];
extern UNIT sim_stop_unit;
extern UNIT sim_internal_timer_unit;
extern UNIT sim_throttle_unit;
extern int32_t sim_internal_timer_time;

/* Restore timer globals to startup-like defaults for isolated unit tests.
   Production code should use the normal timer lifecycle APIs instead. */
void sim_timer_reset_test_state(void);

/* Snapshot throttle internals for deterministic state-machine tests. */
void sim_timer_get_throttle_test_state(
    struct sim_timer_throttle_test_state *state);

t_stat sim_throt_svc(UNIT *uptr);
t_stat sim_timer_show_catchup(FILE *st, UNIT *uptr, int32_t val,
                              const void *desc);

#endif
