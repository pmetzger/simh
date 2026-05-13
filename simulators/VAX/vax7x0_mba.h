/* vax7x0_mba.h: VAX Massbus adapter declarations */
// SPDX-FileCopyrightText: 2004-2008 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_7X0_MBA_H_
#define VAX_7X0_MBA_H_ 1

#include <stdint.h>

int32_t mba_rdbufW(uint32_t mbus, int32_t bc, uint16_t *buf);
int32_t mba_wrbufW(uint32_t mbus, int32_t bc, const uint16_t *buf);
int32_t mba_chbufW(uint32_t mbus, int32_t bc, uint16_t *buf);
int32_t mba_get_bc(uint32_t mbus);
void init_mbus_tab(void);
t_stat build_mbus_tab(DEVICE *dptr, DIB *dibp);
void mba_upd_ata(uint32_t mbus, uint32_t val);
void mba_set_exc(uint32_t mbus);
void mba_set_don(uint32_t mbus);
void mba_set_enbdis(DEVICE *dptr);
t_stat mba_show_num(FILE *st, UNIT *uptr, int32_t val, const void *desc);

#endif /* VAX_7X0_MBA_H_ */
