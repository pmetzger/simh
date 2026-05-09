#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "test_cmocka.h"

#include "sim_video.h"

static void assert_stream_text(FILE *stream, const char *expected)
{
    char buf[128];

    rewind(stream);
    assert_non_null(fgets(buf, sizeof(buf), stream));
    assert_string_equal(buf, expected);
}

/* Verify the non-SDL video implementation reports unavailable features. */
static void test_no_video_stubs_report_unavailable(void **state)
{
    FILE *stream;
    VID_DISPLAY *display = (VID_DISPLAY *)0x1;
    SIM_KEY_EVENT key_event = {0};
    SIM_MOUSE_EVENT mouse_event = {0};
    uint32 pixel = 0x12345678;

    (void)state;

    assert_int_equal(vid_open(NULL, "test", 640, 480, 0), SCPE_NOFNC);
    assert_int_equal(vid_open_window(&display, NULL, "test", 640, 480, 0),
                     SCPE_NOFNC);
    assert_null(display);

    assert_int_equal(vid_close(), SCPE_OK);
    assert_int_equal(vid_close_all(), SCPE_OK);
    assert_int_equal(vid_close_window(NULL), SCPE_OK);

    assert_int_equal(vid_poll_kb(&key_event), SCPE_EOF);
    assert_int_equal(vid_poll_mouse(&mouse_event), SCPE_EOF);
    assert_int_equal(vid_map_rgb(1, 2, 3), 0);
    assert_int_equal(vid_map_rgb_window(NULL, 1, 2, 3), 0);

    vid_draw(1, 2, 3, 4, &pixel);
    vid_draw_window(NULL, 1, 2, 3, 4, &pixel);
    vid_refresh();
    vid_refresh_window(NULL);
    vid_beep();
    vid_set_cursor_position(1, 2);
    vid_set_cursor_position_window(NULL, 1, 2);
    vid_set_window_size(NULL, 640, 480);
    vid_render_set_logical_size(NULL, 320, 240);

    assert_int_equal(vid_set_cursor(false, 0, 0, NULL, NULL, 0, 0), SCPE_NOFNC);
    assert_int_equal(vid_set_cursor_window(NULL, false, 0, 0, NULL, NULL, 0, 0),
                     SCPE_NOFNC);
    assert_false(vid_is_fullscreen());
    assert_false(vid_is_fullscreen_window(NULL));
    assert_int_equal(vid_set_fullscreen(true), SCPE_OK);
    assert_int_equal(vid_set_fullscreen_window(NULL, true), SCPE_OK);

    assert_string_equal(vid_version(), "No Video Support");
    assert_string_equal(vid_key_name(SIM_KEY_A), "");

    stream = tmpfile();
    assert_non_null(stream);
    assert_int_equal(vid_set_release_key(stream, NULL, 0, NULL), SCPE_NOFNC);
    assert_int_equal(vid_show_release_key(stream, NULL, 0, NULL), SCPE_OK);
    assert_stream_text(stream, "no release key");
    fclose(stream);

    stream = tmpfile();
    assert_non_null(stream);
    assert_int_equal(vid_show_video(stream, NULL, 0, NULL), SCPE_OK);
    assert_stream_text(stream, "video support unavailable\n");
    fclose(stream);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_no_video_stubs_report_unavailable),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
