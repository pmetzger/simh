/* pdp11_dup.h: PDP-11 DUP11 bit synchronous shared device packet interface interface

   Copyright (c) 2013, Mark Pizzolato

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   ROBERT M SUPNIK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of Robert M Supnik shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   dup          DUP11 Unibus/DPV11 Qbus bit synchronous interface

   This module describes the interfaces exposed by the dup device for use by
   a packet delivery devices (i.e. KMC11).

   31-May-13    MP      Initial implementation
*/

#ifndef PDP11_DUP_H_
#define PDP11_DUP_H_    1

#include <stdbool.h>
#include <stdint.h>

typedef void (*PACKET_DATA_AVAILABLE_CALLBACK)(int32_t dup, int len);
typedef void (*PACKET_TRANSMIT_COMPLETE_CALLBACK)(int32_t dup, int status);
typedef void (*MODEM_CHANGE_CALLBACK)(int32_t dup);

int32_t dup_get_DSR (int32_t dup);
int32_t dup_get_DCD (int32_t dup);
int32_t dup_get_CTS (int32_t dup);
int32_t dup_get_RING (int32_t dup);
int32_t dup_get_RCVEN (int32_t dup);
t_stat dup_set_DTR (int32_t dup, bool state);
t_stat dup_set_RTS (int32_t dup, bool state);
t_stat dup_set_W3_option (int32_t dup, bool state);
t_stat dup_set_W5_option (int32_t dup, bool state);
t_stat dup_set_W6_option (int32_t dup, bool state);
t_stat dup_set_RCVEN (int32_t dup, bool state);
t_stat dup_setup_dup (int32_t dup, bool enable, bool protocol_DDCMP, bool crc_inhibit, bool halfduplex, uint8_t station);
t_stat dup_reset_dup (int32_t dup);

int32_t dup_csr_to_linenum (int32_t CSRPA);

void dup_set_callback_mode (int32_t dup, PACKET_DATA_AVAILABLE_CALLBACK receive, PACKET_TRANSMIT_COMPLETE_CALLBACK transmit, MODEM_CHANGE_CALLBACK modem);

bool dup_put_msg_bytes (int32_t dup, uint8_t *bytes, size_t len, bool start, bool end);

t_stat dup_get_packet (int32_t dup, const uint8_t **pbuf, uint16_t *psize);

#endif /* PDP11_DUP_H_ */
