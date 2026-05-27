/*
 * $Id: ws.h,v 1.17 2004/02/03 21:23:51 phil Exp $
 * Interfaces to window-system specific code for XY display simulation
 */

/*
 * Copyright (c) 2003-2004, Philip L. Budne
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the names of the authors shall
 * not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization
 * from the authors.
 */

/* unless you're writing a new driver, you shouldn't be looking here! */

int ws_init(const char *, int, int, int, void *);
void ws_shutdown(void);
void *ws_color_rgb(int, int, int);
void *ws_color_black(void);
void *ws_color_white(void);
void ws_display_point(int, int, void *);
void ws_sync(void);
int ws_poll(int *, int);
void ws_beep(void);

/* entries into display.c from below: */
void display_keyup(int);
void display_keydown(int);
void display_repaint(void);

/*
 * Globals set by O/S display level to SCALED location in display
 * coordinate system in order to save an upcall on every mouse
 * movement.
 *
 * *NOT* for consumption by clients of display.c; although display
 * clients can now get the scaling factor, real displays only give you
 * a light pen "hit" when the beam passes under the light pen.
 */

extern int ws_lp_x, ws_lp_y;

/*
 * O/S services in theory independent of window system,
 * but in (current) practice not!
 */
unsigned long os_elapsed(void);
