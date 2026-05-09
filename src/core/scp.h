/* scp.h: simulator control program headers */
// SPDX-FileCopyrightText: 1993-2009 Robert M Supnik
// SPDX-License-Identifier: X11

/*
   05-Dec-10    MP      Added macro invocation of sim_debug
   09-Aug-06    JDB     Added assign_device and deassign_device
   14-Jul-06    RMS     Added sim_activate_abs
   06-Jan-06    RMS     Added fprint_stopped_gen
                        Changed arg type in sim_brk_test
   07-Feb-05    RMS     Added ASSERT command
   09-Sep-04    RMS     Added reset_all_p
   14-Feb-04    RMS     Added debug prototypes (from Dave Hittner)
   02-Jan-04    RMS     Split out from SCP
*/

#ifndef SIM_SCP_H_
#define SIM_SCP_H_     0

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "sim_defs.h"
#include "sim_types.h"

#include "sim_fio.h"
#include "scp_breakpoint.h"
#include "scp_cmdvars.h"
#include "scp_context.h"
#include "scp_expect.h"
#include "scp_expr.h"
#include "scp_help_engine.h"
#include "scp_help.h"
#include "scp_parse.h"
#include "scp_size.h"
#include "scp_unit.h"
#include <sys/stat.h>

/* run_cmd parameters */

#define RU_RUN          0                               /* run */
#define RU_GO           1                               /* go */
#define RU_STEP         2                               /* step */
#define RU_NEXT         3                               /* step or step/over */
#define RU_CONT         4                               /* continue */
#define RU_BOOT         5                               /* boot */

/* exdep_cmd parameters */

#define EX_D            0                               /* deposit */
#define EX_E            1                               /* examine */
#define EX_I            2                               /* interactive */

/* brk_cmd parameters */

#define SSH_ST          0                               /* set */
#define SSH_SH          1                               /* show */
#define SSH_CL          2                               /* clear */

/* get_sim_opt parameters */

#define CMD_OPT_SW      001                             /* switches */
#define CMD_OPT_OF      002                             /* output file */
#define CMD_OPT_SCH     004                             /* search */
#define CMD_OPT_DFT     010                             /* defaults */

/* Shared SCP limits and debug-bit assignments used by helper modules. */
#define MAX_DO_NEST_LVL    20

/* Max width of a value expressed as a formatted string. */
#define MAX_WIDTH ((CHAR_BIT * sizeof (t_value) * 4 + 3) / 3)

#define SIM_DBG_EVENT_NEG   0x80000000      /* negative event dispatch */
#define SIM_DBG_EVENT       0x40000000      /* event dispatch */
#define SIM_DBG_ACTIVATE    0x20000000      /* queue insertion */
#define SIM_DBG_AIO_QUEUE   0x10000000      /* async event queue */
#define SIM_DBG_EXP_STACK   0x08000000      /* expression stack */
#define SIM_DBG_EXP_EVAL    0x04000000      /* expression evaluation */
#define SIM_DBG_BRK_ACTION  0x02000000      /* action processing */
#define SIM_DBG_DO          0x01000000      /* DO command handling */
#define SIM_DBG_SAVE        0x00800000      /* save processing */
#define SIM_DBG_RESTORE     0x00400000      /* restore processing */
#define SCP_LOG_TESTING     0x00200000      /* debug-log self-test */

/* Command processors */

t_stat reset_cmd (int32_t flag, const char *ptr);
t_stat exdep_cmd (int32_t flag, const char *ptr);
t_stat exdep_reg_loop (FILE *ofile, SCHTAB *schptr, int32_t flag,
    const char *cptr, REG *lowr, REG *highr, uint32_t lows, uint32_t highs);
t_stat eval_cmd (int32_t flag, const char *ptr);
t_stat load_cmd (int32_t flag, const char *ptr);
t_stat run_cmd (int32_t flag, const char *ptr);
void run_cmd_message (const char *unechod_cmdline, t_stat r);
t_stat attach_cmd (int32_t flag, const char *ptr);
t_stat detach_cmd (int32_t flag, const char *ptr);
t_stat assign_cmd (int32_t flag, const char *ptr);
t_stat deassign_cmd (int32_t flag, const char *ptr);
t_stat save_cmd (int32_t flag, const char *ptr);
t_stat restore_cmd (int32_t flag, const char *ptr);
t_stat exit_cmd (int32_t flag, const char *ptr);
t_stat set_cmd (int32_t flag, const char *ptr);
t_stat show_cmd (int32_t flag, const char *ptr);
t_stat set_default_cmd (int32_t flg, const char *cptr);
t_stat pwd_cmd (int32_t flg, const char *cptr);
t_stat dir_cmd (int32_t flg, const char *cptr);
t_stat type_cmd (int32_t flg, const char *cptr);
t_stat delete_cmd (int32_t flg, const char *cptr);
t_stat copy_cmd (int32_t flg, const char *cptr);
t_stat rename_cmd (int32_t flg, const char *cptr);
t_stat mkdir_cmd (int32_t flg, const char *cptr);
t_stat rmdir_cmd (int32_t flg, const char *cptr);
t_stat brk_cmd (int32_t flag, const char *ptr);
t_stat do_cmd (int32_t flag, const char *ptr);
t_stat goto_cmd (int32_t flag, const char *ptr);
t_stat return_cmd (int32_t flag, const char *ptr);
t_stat shift_cmd (int32_t flag, const char *ptr);
t_stat call_cmd (int32_t flag, const char *ptr);
t_stat on_cmd (int32_t flag, const char *ptr);
t_stat noop_cmd (int32_t flag, const char *ptr);
t_stat assert_cmd (int32_t flag, const char *ptr);
t_stat sleep_cmd (int32_t flag, const char *ptr);
t_stat help_cmd (int32_t flag, const char *ptr);
t_stat screenshot_cmd (int32_t flag, const char *ptr);
t_stat spawn_cmd (int32_t flag, const char *ptr);
t_stat echo_cmd (int32_t flag, const char *ptr);
t_stat echof_cmd (int32_t flag, const char *ptr);
t_stat debug_cmd (int32_t flag, const char *ptr);
t_stat runlimit_cmd (int32_t flag, const char *ptr);
t_stat tar_cmd (int32_t flag, const char *ptr);
t_stat curl_cmd (int32_t flag, const char *ptr);
t_stat test_lib_cmd (int32_t flag, const char *ptr);

/* Utility routines */

t_stat sim_process_event (void);
t_stat sim_activate (UNIT *uptr, int32_t interval);
t_stat _sim_activate (UNIT *uptr, int32_t interval);
t_stat sim_activate_abs (UNIT *uptr, int32_t interval);
t_stat sim_activate_notbefore (UNIT *uptr, int32_t rtime);
t_stat sim_activate_after (UNIT *uptr, uint32_t usecs_walltime);
t_stat sim_activate_after_d (UNIT *uptr, double usecs_walltime);
t_stat _sim_activate_after (UNIT *uptr, double usecs_walltime);
t_stat sim_activate_after_abs (UNIT *uptr, uint32_t usecs_walltime);
t_stat sim_activate_after_abs_d (UNIT *uptr, double usecs_walltime);
t_stat _sim_activate_after_abs (UNIT *uptr, double usecs_walltime);
t_stat sim_cancel (UNIT *uptr);
bool sim_is_active (UNIT *uptr);
int32_t sim_activate_time (UNIT *uptr);
int32_t _sim_activate_queue_time (UNIT *uptr);
int32_t _sim_activate_time (UNIT *uptr);
double sim_activate_time_usecs (UNIT *uptr);
t_stat sim_run_boot_prep (int32_t flag);
double sim_gtime (void);
uint32_t sim_grtime (void);
int32_t sim_qcount (void);
t_stat assign_device (DEVICE *dptr, const char *cptr);
t_stat deassign_device (DEVICE *dptr);
t_stat reset_all (uint32_t start_device);
t_stat reset_all_p (uint32_t start_device);
t_stat set_writelock (UNIT *uptr, int32_t val, const char *cptr, void *desc);
t_stat show_writelock (FILE *st, UNIT *uptr, int32_t val, const void *desc);
bool get_yn (const char *ques, bool deflt);
void sim_srand (uint_t seed);
int sim_rand (void);
#ifdef RAND_MAX
#undef RAND_MAX
#endif
#define RAND_MAX 2147483646
#define rand sim_rand
#define srand(seed) sim_srand(seed)
const char *get_sim_opt (int32_t opt, const char *cptr, t_stat *st);
const char *put_switches (char *buf, size_t bufsize, uint32_t sw);
t_value get_uint (const char *cptr, uint32_t radix, t_value max, t_stat *status);
const char *get_range (DEVICE *dptr, const char *cptr, t_addr *lo, t_addr *hi,
    uint32_t rdx, t_addr max, char term);
t_stat sim_decode_quoted_string (const char *iptr, uint8_t *optr, uint32_t *osize);
char *sim_encode_quoted_string (const uint8_t *iptr, size_t size);
void fprint_buffer_string (FILE *st, const uint8_t *buf, size_t size);
t_value strtotv (const char *cptr, const char **endptr, uint32_t radix);
t_svalue strtotsv (const char *inptr, const char **endptr, uint32_t radix);
int Fprintf (FILE *f, const char *fmt, ...) PRINTF_FMT(2, 3);
/* Use scp.c provided fprintf function */
#define fprintf Fprintf
#define fputs(_s,_f) Fprintf(_f,"%s",_s)
#define fputc(_c,_f) Fprintf(_f,"%c",_c)
t_stat sim_set_memory_load_file (const uchar_t *data, size_t size);
int Fgetc (FILE *f);
t_stat fprint_val (FILE *stream, t_value val, uint32_t rdx, uint32_t wid, uint32_t fmt);
t_stat sprint_val (char *buf, t_value val, uint32_t rdx, size_t wid, uint32_t fmt);
t_stat sim_print_val (t_value val, uint32_t radix, uint32_t width, uint32_t format);
const char *sim_fmt_secs (double seconds);
const char *sim_fmt_numeric (double number);
const char *sprint_capac (DEVICE *dptr, UNIT *uptr);
const char *sim_regex_backend_name (void);
const char *sim_regex_backend_version (void);
void sim_fprint_regex_support (FILE *st);
REG *find_reg_glob (const char *ptr, const char **optr, DEVICE **gdptr);
char *read_line (char *cptr, int32_t size, FILE *stream);
char *read_line_p (const char *prompt, char *ptr, int32_t size, FILE *stream);
CTAB *find_cmd (const char *gbuf);
CTAB *scp_cmd_table (void);
const char *scp_argv0 (void);
bool scp_has_oline (void);
bool scp_do_echo_enabled(void);
int32_t scp_do_depth(void);
const char *_get_dbg_verb(uint32_t dbits, DEVICE *dptr, UNIT *uptr);
REG *find_reg (const char *ptr, const char **optr, DEVICE *dptr);
CTAB *find_ctab (CTAB *tab, const char *gbuf);
C1TAB *find_c1tab (C1TAB *tab, const char *gbuf);
SHTAB *find_shtab (SHTAB *tab, const char *gbuf);
t_stat get_aval (t_addr addr, DEVICE *dptr, UNIT *uptr);
t_value get_rval (REG *rptr, uint32_t idx);
const char *match_ext (const char *fnam, const char *ext);
int sim_cmp_string (const char *s1, const char *s2);
t_stat show_version (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
t_stat set_dev_debug (DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
t_stat show_dev_debug (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
t_stat sim_add_debug_flags (DEVICE *dptr, DEBTAB *debflags);
const char *sim_error_text (t_stat stat);
t_stat sim_string_to_stat (const char *cptr, t_stat *cond);
t_stat sim_sched_step (void);
t_stat sim_cancel_step (void);
int scp_main (int argc, char *argv[]);
const char *sim_get_tool_path (const char *tool);
void sim_printf (const char *fmt, ...) PRINTF_FMT(1, 2);
void sim_perror (const char *msg);
t_stat sim_call_argv (int (*main_like)(int argc, char *argv[]), const char *cptr);
t_stat sim_messagef (t_stat stat, const char *fmt, ...) PRINTF_FMT(2, 3);
void sim_data_trace(DEVICE *dptr, UNIT *uptr, const uint8_t *data, const char *position, size_t len, const char *txt, uint32_t reason);
void sim_debug_bits_hdr (uint32_t dbits, DEVICE* dptr, const char *header,
    BITFIELD* bitdefs, uint32_t before, uint32_t after, int terminate);
void sim_debug_bits (uint32_t dbits, DEVICE* dptr, BITFIELD* bitdefs,
    uint32_t before, uint32_t after, int terminate);
void _sim_vdebug (uint32_t dbits, DEVICE* dptr, UNIT *uptr, const char* fmt,
    va_list arglist) PRINTF_FMT(4, 0);
void _sim_debug_unit (uint32_t dbits, UNIT *uptr, const char* fmt, ...)
    PRINTF_FMT(3, 4);
void _sim_debug_device (uint32_t dbits, DEVICE* dptr, const char* fmt, ...)
    PRINTF_FMT(3, 4);
#define sim_debug(dbits, dptr, ...) do { if ((sim_deb != NULL) && ((dptr) != NULL) && ((dptr)->dctrl & (dbits))) _sim_debug_device (dbits, dptr, __VA_ARGS__);} while (0)
#define sim_debug_unit(dbits, uptr, ...) do { if ((sim_deb != NULL) && ((uptr) != NULL) && (uptr->dptr != NULL) && (((uptr)->dctrl | (uptr)->dptr->dctrl) & (dbits))) _sim_debug_unit (dbits, uptr, __VA_ARGS__);} while (0)
void sim_flush_buffered_files (void);

void fprint_stopped_gen (FILE *st, t_stat v, REG *pc, DEVICE *dptr);
#define SCP_HELP_FLAT   (1u << 31)       /* Force flat help when prompting is not possible */
#define SCP_HELP_ONECMD (1u << 30)       /* Display one topic, do not prompt */
#define SCP_HELP_ATTACH (1u << 29)       /* Top level topic is ATTACH help */

/* Global data */

extern DEVICE *sim_dflt_dev;
extern DEVICE sim_scp_dev;
extern int32_t sim_interval;
extern int32_t sim_switches;
extern int32_t sim_switch_number;
#define GET_SWITCHES(cp) \
    if ((cp = get_sim_sw (cp)) == NULL) return SCPE_INVSW
#define GET_RADIX(val,dft) \
    if (sim_switches & SWMASK ('O')) val = 8; \
    else if (sim_switches & SWMASK ('D')) val = 10; \
    else if (sim_switches & SWMASK ('H')) val = 16; \
    else if ((sim_switch_number >= 2) && (sim_switch_number <= 36)) val = sim_switch_number; \
    else val = dft;
extern int32_t sim_show_message;
extern int32_t sim_quiet;
extern int32_t sim_step;
extern t_stat sim_last_cmd_stat;                        /* Command Status */
extern FILE *sim_log;                                   /* log file */
extern FILEREF *sim_log_ref;                            /* log file file reference */
extern FILE *sim_deb;                                   /* debug file */
extern FILEREF *sim_deb_ref;                            /* debug file file reference */
extern int32_t sim_deb_switches;                        /* debug display flags */
extern size_t sim_deb_buffer_size;                      /* debug memory buffer size */
extern char *sim_deb_buffer;                            /* debug memory buffer */
extern size_t sim_debug_buffer_offset;                  /* debug memory buffer insertion offset */
extern size_t sim_debug_buffer_inuse;                   /* debug memory buffer inuse count */
extern struct timespec sim_deb_basetime;                /* debug base time for relative time output */
extern UNIT *sim_clock_queue;
extern volatile bool sim_is_running;
extern bool sim_processing_event;                       /* Called from sim_process_event */
extern char *sim_prompt;                                /* prompt string */
extern const char *sim_savename;                        /* Simulator Name used in Save/Restore files */
extern t_value *sim_eval;
extern volatile bool stop_cpu;
extern const char *sim_prog_name;                       /* executable program name */
extern FILE *stdnul;
extern bool sim_asynch_enabled;
#if defined(SIM_ASYNCH_IO)
int sim_aio_update_queue (void);
void sim_aio_activate (ACTIVATE_API caller, UNIT *uptr, int32_t event_time);
#endif

/* VM interface */

extern char sim_name[64];
extern const char *sim_vm_release;
extern const char *sim_vm_release_message;
extern DEVICE *sim_devices[];
extern REG *sim_PC;
extern const char *sim_stop_messages[SCPE_BASE];
extern t_stat sim_instr (void);
extern t_stat sim_load (FILE *ptr, const char *cptr, const char *fnam, int flag);
extern int32_t sim_emax;
extern t_stat fprint_sym (FILE *ofile, t_addr addr, t_value *val,
    UNIT *uptr, int32_t sw);
extern t_stat parse_sym (const char *cptr, t_addr addr, UNIT *uptr, t_value *val,
    int32_t sw);

/* The per-simulator init routine is a weak global that defaults to NULL
   The other per-simulator pointers can be overridden by the init routine

extern void (*sim_vm_init) (void);

   This routine is no longer invoked this way since it doesn't work reliably
   on all simh supported compile environments.  A simulator that needs these
   initializations can perform them in the CPU device reset routine which will
   always be called before anything else can be processed.

 */
extern char *(*sim_vm_read) (char *ptr, int32_t size, FILE *stream);
extern void (*sim_vm_post) (bool from_scp);
extern CTAB *sim_vm_cmd;
extern void (*sim_vm_sprint_addr) (char *buf, DEVICE *dptr, t_addr addr);
extern void (*sim_vm_fprint_addr) (FILE *st, DEVICE *dptr, t_addr addr);
extern t_addr (*sim_vm_parse_addr) (DEVICE *dptr, const char *cptr, const char **tptr);
extern bool (*sim_vm_fprint_stopped) (FILE *st, t_stat reason);
extern t_value (*sim_vm_pc_value) (void);
extern bool (*sim_vm_is_subroutine_call) (t_addr **ret_addrs);
extern void (*sim_vm_reg_update) (REG *rptr, uint32_t idx, t_value prev_val, t_value new_val);
extern const char **sim_clock_precalibrate_commands;
extern int32_t sim_vm_initial_ips;                      /* base estimate of simulated instructions per second */
extern const char *sim_vm_interval_units;               /* Simulator can change this - default "instructions" */
extern const char *sim_vm_step_unit;                    /* Simulator can change this - default "instruction" */


/* Core SCP libraries can potentially have unit test routines.
   These defines help implement consistent unit test functionality */

#define SIM_TEST_INIT                                           \
        volatile int test_stat;                                 \
        const char *volatile sim_test;                          \
        jmp_buf sim_test_env;                                   \
        if ((test_stat = setjmp (sim_test_env))) {              \
            sim_printf ("Error: %d - '%s' processing: %s\n",    \
                        SCPE_BARE_STATUS(test_stat),            \
                        sim_error_text(test_stat), sim_test);   \
            return test_stat;                                   \
            }
#define SIM_TEST(_stat)                                         \
        do {                                                    \
            if (SCPE_OK != (test_stat = (_stat))) {             \
                sim_test = #_stat;                              \
                longjmp (sim_test_env, test_stat);              \
                }                                               \
            } while (0)


#endif
