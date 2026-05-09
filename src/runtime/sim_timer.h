/* sim_timer.h: simulator timer library headers */
// SPDX-FileCopyrightText: 1993-2008 Robert M Supnik
// SPDX-License-Identifier: X11

/*
   28-Apr-07    RMS     Added sim_rtc_init_all
   17-Oct-06    RMS     Added idle support
   02-Jan-04    RMS     Split out from SCP
*/

#ifndef SIM_TIMER_H_
#define SIM_TIMER_H_   0

#include <stdbool.h>
#include <stdint.h>

#include "sim_time.h"
#include "sim_types.h"
#if defined(SIM_ASYNCH_IO) || defined(USE_READER_THREAD)
#include <pthread.h>
#endif


#define SIM_NTIMERS     8                           /* # timers */
#define SIM_TMAX        500                         /* max timer makeup */

#define SIM_INITIAL_IPS 5000000                     /* uncalibrated assumption */
                                                    /* about instructions per second */
#define SIM_PRE_CALIBRATE_MIN_MS    100             /* minimum time to run precalibration activities */

#define SIM_IDLE_CAL    10                          /* ms to calibrate */
#define SIM_IDLE_STMIN  2                           /* min sec for stability */
#define SIM_IDLE_STDFLT 20                          /* dft sec for stability */
#define SIM_IDLE_STMAX  600                         /* max sec for stability */

#define SIM_THROT_WINIT           1000              /* cycles to skip */
#define SIM_THROT_WST             10000             /* initial wait */
#define SIM_THROT_WMUL            4                 /* multiplier */
#define SIM_THROT_WMIN            50                /* min wait */
#define SIM_THROT_DRIFT_PCT_DFLT  5                 /* drift percentage for recalibrate */
#define SIM_THROT_MSMIN           10                /* min for measurement */
#define SIM_THROT_NONE            0                 /* throttle parameters */
#define SIM_THROT_MCYC            1                 /* MegaCycles Per Sec */
#define SIM_THROT_KCYC            2                 /* KiloCycles Per Sec */
#define SIM_THROT_PCT             3                 /* Max Percent of host CPU */
#define SIM_THROT_SPC             4                 /* Specific periodic Delay */
#define SIM_THROT_STATE_INIT      0                 /* Starting */
#define SIM_THROT_STATE_TIME      1                 /* Checking Time */
#define SIM_THROT_STATE_THROTTLE  2                 /* Throttling  */

#define TIMER_DBG_IDLE  0x001                       /* Debug Flag for Idle Debugging */
#define TIMER_DBG_QUEUE 0x002                       /* Debug Flag for Asynch Queue Debugging */
#define TIMER_DBG_MUX   0x004                       /* Debug Flag for Asynch Queue Debugging */

bool sim_timer_init (void);
void sim_timespec_diff (struct timespec *diff, struct timespec *min, struct timespec *sub);
int sim_timer_deadline_msec (struct timespec *deadline, uint_t msec);
double sim_timenow_double (void);
int32_t sim_rtcn_init (int32_t time, int32_t tmr);
int32_t sim_rtcn_init_unit (UNIT *uptr, int32_t time, int32_t tmr);
int32_t sim_rtcn_init_unit_ticks (UNIT *uptr, int32_t time, int32_t tmr, int32_t ticksper);
void sim_rtcn_get_time (struct timespec *now, int tmr);
time_t sim_get_time (time_t *now);
t_stat sim_rtcn_tick_ack (uint32_t time, int32_t tmr);
void sim_rtcn_init_all (void);
int32_t sim_rtcn_calb (uint32_t ticksper, int32_t tmr);
int32_t sim_rtcn_calb_tick (int32_t tmr);
int32_t sim_rtc_init (int32_t time);
int32_t sim_rtc_calb (uint32_t ticksper);
t_stat sim_set_timers (int32_t arg, const char *cptr);
t_stat sim_show_timers (FILE* st, DEVICE *dptr, UNIT* uptr, int32_t val, const char* desc);
t_stat sim_show_clock_queues (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
bool sim_idle (uint32_t tmr, int sin_cyc);
t_stat sim_set_throt (int32_t arg, const char *cptr);
t_stat sim_show_throt (FILE *st, DEVICE *dnotused, UNIT *unotused, int32_t flag, const char *cptr);
t_stat sim_set_idle (UNIT *uptr, int32_t val, const char *cptr, void *desc);
t_stat sim_clr_idle (UNIT *uptr, int32_t val, const char *cptr, void *desc);
t_stat sim_show_idle (FILE *st, UNIT *uptr, int32_t val, const void *desc);
void sim_throt_sched (void);
void sim_throt_cancel (void);

/* Return the current host time as a wrapping millisecond tick counter.
   The uint32_t value intentionally wraps; callers must compare tick values
   with unsigned subtraction rather than ordering raw timestamps. */
uint32_t sim_os_msec (void);

void sim_os_sleep (uint_t sec);
uint32_t sim_os_ms_sleep (uint_t msec);
uint32_t sim_os_ms_sleep_init (void);
void sim_start_timer_services (void);
void sim_stop_timer_services (void);
t_stat sim_timer_change_asynch (void);
t_stat sim_timer_activate (UNIT *uptr, int32_t interval);
t_stat sim_timer_activate_after (UNIT *uptr, double usec_delay);
int32_t _sim_timer_activate_time (UNIT *uptr);
double sim_timer_activate_time_usecs (UNIT *uptr);
bool sim_timer_is_active (UNIT *uptr);
t_stat sim_timer_cancel (UNIT *uptr);
t_stat sim_register_clock_unit (UNIT *uptr);
t_stat sim_register_clock_unit_tmr (UNIT *uptr, int32_t tmr);
t_stat sim_clock_coschedule (UNIT *uptr, int32_t interval);
t_stat sim_clock_coschedule_abs (UNIT *uptr, int32_t interval);
t_stat sim_clock_coschedule_tmr (UNIT *uptr, int32_t tmr, int32_t ticks);
t_stat sim_clock_coschedule_tmr_abs (UNIT *uptr, int32_t tmr, int32_t ticks);
double sim_timer_inst_per_sec (void);
void sim_timer_precalibrate_execution_rate (void);
int32_t sim_rtcn_tick_size (int32_t tmr);
int32_t sim_rtcn_calibrated_tmr (void);
bool sim_timer_idle_capable (uint32_t *host_ms_sleep_1, uint32_t *host_tick_ms);
#define PRIORITY_BELOW_NORMAL  -1
#define PRIORITY_NORMAL         0
#define PRIORITY_ABOVE_NORMAL   1
t_stat sim_os_set_thread_priority (int below_normal_above);
uint32_t sim_get_rom_delay_factor (void);
void sim_set_rom_delay_factor (uint32_t delay);
uint32_t sim_rom_read_with_delay (uint32_t val);
double sim_host_speed_factor (void);

extern bool sim_idle_enab;                          /* idle enabled flag */
extern volatile bool sim_idle_wait;                 /* idle waiting flag */
extern bool sim_asynch_timer;
extern DEVICE sim_timer_dev;
extern UNIT * volatile sim_clock_cosched_queue[SIM_NTIMERS+1];
extern const bool rtc_avail;

#endif
