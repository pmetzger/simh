/* sim_serial.h: OS-dependent serial port routines header file */
// SPDX-FileCopyrightText: 2008 J. David Bryan, Mark Pizzolato
// SPDX-License-Identifier: X11

/*
   07-Oct-08    JDB     [serial] Created file
   22-Apr-12    MP      Adapted from code originally written by J. David Bryan

*/


#ifndef SIM_SERIAL_H_
#define SIM_SERIAL_H_    0

#ifndef SIMH_SERHANDLE_DEFINED
#define SIMH_SERHANDLE_DEFINED 0
typedef struct SERPORT *SERHANDLE;
#endif /* SERHANDLE_DEFINED */

#if defined (_WIN32)                        /* Windows definitions */

/* We need the basic Win32 definitions, but including "windows.h" also includes
   "winsock.h" as well.  However, "sim_sock.h" explicitly includes "winsock2.h,"
   and this file cannot coexist with "winsock.h".  So we set a guard definition
   that prevents "winsock.h" from being included.
*/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#if !defined(INVALID_HANDLE)
#define INVALID_HANDLE  (SERHANDLE)INVALID_HANDLE_VALUE
#endif /* !defined(INVALID_HANDLE) */

#elif defined (__unix__) || defined (__APPLE__)         /* POSIX definitions */

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#if !defined(INVALID_HANDLE)
#define INVALID_HANDLE  ((SERHANDLE)(void *)-1)
#endif /* !defined(INVALID_HANDLE) */

#else                                           /* Non-implemented definitions */

#if !defined(INVALID_HANDLE)
#define INVALID_HANDLE  ((SERHANDLE)(void *)-1)
#endif /* !defined(INVALID_HANDLE) */

#endif  /* OS variants */


/* Common definitions */

/* Global routines */
#include <stdint.h>

#include "sim_tmxr.h"                           /* need TMLN definition and modem definitions */

extern SERHANDLE sim_open_serial    (char *name, TMLN *lp, t_stat *status);
extern t_stat    sim_config_serial  (SERHANDLE port, const char *config);
extern t_stat    sim_control_serial (SERHANDLE port, int32_t bits_to_set, int32_t bits_to_clear, int32_t *incoming_bits);
extern int32_t   sim_read_serial    (SERHANDLE port, char *buffer, int32_t count, char *brk);
extern int32_t   sim_write_serial   (SERHANDLE port, char *buffer, int32_t count);
extern void      sim_close_serial   (SERHANDLE port);
extern t_stat    sim_show_serial    (FILE* st, DEVICE *dptr, UNIT* uptr, int32_t val, const char* desc);

#endif
