#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "display.h"
#include "vt11.h"
#include "vtmacs.h"

enum {
    DJSR_ABS_OPCODE = 0162000,
    DPOP_NR_OPCODE = 0165000,
    DPOP_R_OPCODE = 0166000,
};

static vt11word display_file[16];
static unsigned stop_interrupts;
static unsigned light_pen_interrupts;
static unsigned character_interrupts;
static unsigned name_interrupts;

uchar_t display_lp_sw = 0;
uchar_t display_tablet = 0;
uchar_t display_last_char = 0;
unsigned long spacewar_switches = 0;

int display_init(enum display_type type, int scale, void *dptr)
{
    (void)type;
    (void)scale;
    (void)dptr;

    return 1;
}

void display_close(void *dptr)
{
    (void)dptr;
}

int display_xpoints(void)
{
    return 1024;
}

int display_ypoints(void)
{
    return 1024;
}

int display_scale(void)
{
    return 1;
}

int display_age(int us, int slowdown)
{
    (void)us;
    (void)slowdown;

    return 0;
}

int display_is_blank(void)
{
    return 0;
}

int display_point(int x, int y, int intensity, int color)
{
    (void)x;
    (void)y;
    (void)intensity;
    (void)color;

    return 0;
}

void display_line(int x1, int y1, int x2, int y2, int intensity)
{
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)intensity;
}

void display_sync(void)
{
}

void display_reset(void)
{
}

void display_beep(void)
{
}

void display_lp_radius(int radius)
{
    (void)radius;
}

void cpu_get_switches(unsigned long *p1, unsigned long *p2)
{
    *p1 = 0;
    *p2 = 0;
}

void cpu_set_switches(unsigned long p1, unsigned long p2)
{
    (void)p1;
    (void)p2;
}

int vt_fetch(uint32_t addr, vt11word *w)
{
    uint32_t index = addr / 2;

    if (index >= sizeof(display_file) / sizeof(display_file[0]))
        return 1;

    *w = display_file[index];
    return 0;
}

void vt_stop_intr(void)
{
    ++stop_interrupts;
}

void vt_lpen_intr(void)
{
    ++light_pen_interrupts;
}

void vt_char_intr(void)
{
    ++character_interrupts;
}

void vt_name_intr(void)
{
    ++name_interrupts;
}

static vt11word set_intensity(unsigned intensity)
{
    return SGM(GM_CHAR, IN_0 + (vt11word)(intensity << 7), LP_SAME, BL_SAME,
               LT_SAME);
}

static unsigned current_intensity(void)
{
    return (unsigned)((vt11_get_mpr() >> 8) & 07);
}

static void reset_vt11_test(void)
{
    memset(display_file, 0, sizeof(display_file));
    stop_interrupts = 0;
    light_pen_interrupts = 0;
    character_interrupts = 0;
    name_interrupts = 0;
    display_lp_sw = 0;
    display_tablet = 0;
    display_last_char = 0;
    spacewar_switches = 0;

    vt11_display = DIS_VR48;
    vt11_scale = RES_FULL;
    vt11_init = 0;
    vt11_set_dpc(0);
}

static void run_cycles(unsigned count)
{
    while (count-- > 0)
        assert_true(vt11_cycle(10, 0));
}

static void load_subroutine_program(vt11word pop_opcode)
{
    display_file[0] = set_intensity(1);
    display_file[1] = DJSR_ABS_OPCODE;
    display_file[2] = 0010;
    display_file[4] = set_intensity(6);
    display_file[5] = pop_opcode;
}

static void test_vtmacs_pop_opcodes_match_vt48_decode(void **state)
{
    (void)state;

    assert_int_equal(DPOP_NR, DPOP_NR_OPCODE);
    assert_int_equal(DPOP_R, DPOP_R_OPCODE);
}

static void test_pop_not_restore_keeps_current_parameters(void **state)
{
    (void)state;

    reset_vt11_test();
    load_subroutine_program(DPOP_NR_OPCODE);

    run_cycles(5);

    assert_int_equal(vt11_get_dpc(), 000006);
    assert_int_equal(current_intensity(), 6);
}

static void test_pop_restore_restores_saved_parameters(void **state)
{
    (void)state;

    reset_vt11_test();
    load_subroutine_program(DPOP_R_OPCODE);

    run_cycles(5);

    assert_int_equal(vt11_get_dpc(), 000006);
    assert_int_equal(current_intensity(), 1);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_vtmacs_pop_opcodes_match_vt48_decode),
        cmocka_unit_test(test_pop_not_restore_keeps_current_parameters),
        cmocka_unit_test(test_pop_restore_restores_saved_parameters),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
