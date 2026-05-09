/* sim_console.h: simulator console I/O library headers */
// SPDX-FileCopyrightText: 1993-2008 Robert M Supnik
// SPDX-License-Identifier: X11

/*
   27-Sep-22    RMS     Added sim_ttisatty
   14-Dec-14    JDB     [4.0] Added sim_*_char externals
   02-Jan-14    RMS     Added tab stop routines
   17-Jan-11    MP      Added buffered line capabilities
   22-Jun-06    RMS     Implemented SET/SHOW PCHAR
   22-Nov-05    RMS     Added central input/output conversion support
   05-Nov-04    RMS     Moved SET/SHOW DEBUG under CONSOLE hierarchy
   28-May-04    RMS     Added SET/SHOW CONSOLE
   02-Jan-04    RMS     Removed timer routines, added Telnet console routines
*/

#ifndef SIM_CONSOLE_H_
#define SIM_CONSOLE_H_ 0

#include <stdbool.h>
#include <stdint.h>

#define TTUF_V_MODE     (UNIT_V_UF + 0)
#define TTUF_W_MODE     2
#define  TTUF_MODE_7B   0
#define  TTUF_MODE_8B   1
#define  TTUF_MODE_UC   2
#define  TTUF_MODE_7P   3
#define TTUF_M_MODE     ((1u << TTUF_W_MODE) - 1)
#define TTUF_V_PAR      (TTUF_V_MODE + TTUF_W_MODE)
#define TTUF_W_PAR      2
#define  TTUF_PAR_SPACE 0
#define  TTUF_PAR_MARK  1
#define  TTUF_PAR_EVEN  2
#define  TTUF_PAR_ODD   3
#define TTUF_M_PAR      ((1u << TTUF_W_PAR) - 1)
#define  TTUF_KSR       (1u << (TTUF_W_MODE + TTUF_W_PAR))
#define TTUF_V_UF       (TTUF_V_MODE + TTUF_W_MODE + TTUF_W_PAR)
#define TT_MODE         (TTUF_M_MODE << TTUF_V_MODE)
#define  TT_MODE_7B     (TTUF_MODE_7B << TTUF_V_MODE)
#define  TT_MODE_8B     (TTUF_MODE_8B << TTUF_V_MODE)
#define  TT_MODE_UC     (TTUF_MODE_UC << TTUF_V_MODE)
#define  TT_MODE_7P     (TTUF_MODE_7P << TTUF_V_MODE)
#define  TT_MODE_KSR    (TT_MODE_UC|TT_PAR_MARK)
/* 7 bit modes allow for an 8th bit parity mode */
#define TT_PAR          (TTUF_M_PAR << TTUF_V_PAR)
#define  TT_PAR_SPACE   (TTUF_PAR_SPACE << TTUF_V_PAR)
#define  TT_PAR_MARK    (TTUF_PAR_MARK  << TTUF_V_PAR)
#define  TT_PAR_EVEN    (TTUF_PAR_EVEN  << TTUF_V_PAR)
#define  TT_PAR_ODD     (TTUF_PAR_ODD   << TTUF_V_PAR)
/* TT_GET_MODE returns both the TT_MODE and TT_PAR fields
   since they together are passed into sim_tt_inpcvt() */
#define TT_GET_MODE(x)  (((x) >> TTUF_V_MODE) & (TTUF_M_MODE | (TTUF_M_PAR << TTUF_W_MODE)))

t_stat sim_set_console (int32_t flag, const char *cptr);
t_stat sim_set_remote_console (int32_t flag, const char *cptr);
void sim_remote_process_command (void);
t_stat sim_set_kmap (int32_t flag, const char *cptr);
t_stat sim_set_telnet (int32_t flag, const char *cptr);
t_stat sim_set_notelnet (int32_t flag, const char *cptr);
t_stat sim_set_serial (int32_t flag, const char *cptr);
t_stat sim_set_noserial (int32_t flag, const char *cptr);
t_stat sim_set_logon (int32_t flag, const char *cptr);
t_stat sim_set_logoff (int32_t flag, const char *cptr);
int32_t sim_set_deb_switches (int32_t switches);
t_stat sim_set_debon (int32_t flag, const char *cptr);
t_stat sim_set_cons_debug (int32_t flg, const char *cptr);
t_stat sim_set_cons_buff (int32_t flg, const char *cptr);
t_stat sim_set_cons_unbuff (int32_t flg, const char *cptr);
t_stat sim_set_cons_log (int32_t flg, const char *cptr);
t_stat sim_set_cons_nolog (int32_t flg, const char *cptr);
t_stat sim_set_deboff (int32_t flag, const char *cptr);
t_stat sim_set_cons_expect (int32_t flg, const char *cptr);
t_stat sim_set_cons_noexpect (int32_t flg, const char *cptr);
t_stat sim_set_pchar (int32_t flag, const char *cptr);
t_stat sim_set_cons_speed (int32_t flag, const char *cptr);
t_stat sim_show_console (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
t_stat sim_show_remote_console (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
t_stat sim_show_kmap (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
t_stat sim_show_telnet (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
t_stat sim_show_log (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
t_stat sim_show_debug (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
t_stat sim_show_pchar (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
t_stat sim_show_cons_speed (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
t_stat sim_show_cons_buff (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
t_stat sim_show_cons_log (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
t_stat sim_show_cons_debug (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
t_stat sim_show_cons_expect (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
t_stat sim_check_console (int32_t sec);
t_stat sim_open_logfile (const char *filename, bool binary, FILE **pf, FILEREF **pref);
t_stat sim_close_logfile (FILEREF **pref);
const char *sim_logfile_name (FILE *st, FILEREF *ref);
SEND *sim_cons_get_send (void);
EXPECT *sim_cons_get_expect (void);
t_stat sim_show_cons_send_input (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
t_stat sim_set_noconsole_port (void);
t_stat sim_set_stable_registers_state (void);
t_stat sim_poll_kbd (void);
t_stat sim_putchar (int32_t c);
t_stat sim_putchar_s (int32_t c);
t_stat sim_ttinit (void);
t_stat sim_ttrun (void);
t_stat sim_ttcmd (void);
t_stat sim_ttclose (void);
bool sim_ttisatty (void);
bool sim_fd_isatty (int fd);
int32_t sim_tt_inpcvt (int32_t c, uint32_t mode);
int32_t sim_tt_outcvt (int32_t c, uint32_t mode);
t_stat sim_tt_set_mode (UNIT *uptr, int32_t val, const char *cptr, void *desc);
t_stat sim_tt_set_parity (UNIT *uptr, int32_t val, const char *cptr, void *desc);
t_stat sim_tt_show_modepar (FILE *st, UNIT *uptr, int32_t val, const void *desc);
t_stat sim_tt_settabs (UNIT *uptr, int32_t val, const char *cptr, void *desc);
t_stat sim_tt_showtabs (FILE *st, UNIT *uptr, int32_t val, const void *desc);
bool sim_is_remote_console_master_line (void *lp);

extern int32_t sim_rem_cmd_active_line; /* command in progress on line # */

extern int32_t sim_int_char;            /* interrupt character */
extern int32_t sim_brk_char;            /* break character */
extern int32_t sim_tt_pchar;            /* printable character mask */
extern int32_t sim_del_char;            /* delete character */
extern bool sim_signaled_int_char;      /* WRU character detected by signal while running  */
extern uint32_t sim_last_poll_kbd_time; /* time when sim_poll_kbd was called */

#endif
