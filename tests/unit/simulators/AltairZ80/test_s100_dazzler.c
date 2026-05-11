#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "test_cmocka.h"

#include "altairz80_defs.h"
#include "sim_video.h"

ChipType chiptype;

uint32_t sim_map_resource(uint32_t baseaddr, uint32_t size,
                          uint32_t resource_type,
                          int32_t (*routine)(const int32_t, const int32_t,
                                             const int32_t),
                          const char *name, uint8_t unmap);
t_stat set_iobase(UNIT *uptr, int32_t val, const char *cptr, void *desc);
t_stat show_iobase(FILE *st, UNIT *uptr, int32_t val, const void *desc);
uint8_t GetBYTEWrapper(const uint32_t Addr);

uint32_t sim_map_resource(uint32_t baseaddr, uint32_t size, uint32_t resource_type,
                        int32_t (*routine)(const int32_t, const int32_t,
                                         const int32_t),
                        const char *name, uint8_t unmap)
{
    (void)baseaddr;
    (void)size;
    (void)resource_type;
    (void)routine;
    (void)name;
    (void)unmap;

    return SCPE_OK;
}

t_stat set_iobase(UNIT *uptr, int32_t val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;

    return SCPE_OK;
}

t_stat show_iobase(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;

    return SCPE_OK;
}

uint8_t GetBYTEWrapper(const uint32_t Addr)
{
    (void)Addr;

    return 0;
}

#include "s100_dazzler.c"

static void reset_dazzler_register_state(void)
{
    daz_vptr = NULL;
    daz_0e = 0x00;
    daz_0f = 0x80;
    daz_addr = 0x0000;
    daz_frame = 0x3f;
    daz_res = 32;
    daz_pages = 1;
    daz_screen_width = 32;
    daz_screen_height = 32;
    daz_screen_pixels = 32 * 32;
    daz_color = 0;
}

static void test_dazzler_control_ports_preserve_byte_registers(void **state)
{
    (void)state;

    reset_dazzler_register_state();

    assert_int_equal(daz_io(DAZ_IO_BASE, 1, 0xff), 0xff);
    assert_int_equal(daz_0e, 0xff);
    assert_int_equal(daz_addr, 0xfe00);

    assert_int_equal(daz_io(DAZ_IO_BASE + 1, 1, 0xf3), 0xff);
    assert_int_equal(daz_0f, 0xf3);
    assert_int_equal(daz_color, 0x03);
    assert_int_equal(daz_pages, 4);
    assert_int_equal(daz_res, 128);
    assert_int_equal(daz_screen_width, 128);
    assert_int_equal(daz_screen_height, 128);
    assert_int_equal(daz_screen_pixels, 128 * 128);
}

static void test_dazzler_frame_status_is_byte_value(void **state)
{
    int32_t frame_status;

    (void)state;

    reset_dazzler_register_state();

    frame_status = daz_io(DAZ_IO_BASE, 0, 0);

    assert_true(frame_status == 0x3f || frame_status == 0x7f ||
                frame_status == 0xff);
    assert_int_equal(daz_frame, frame_status);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_dazzler_control_ports_preserve_byte_registers),
        cmocka_unit_test(test_dazzler_frame_status_is_byte_value),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
