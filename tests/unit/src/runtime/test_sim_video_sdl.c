#include <stddef.h>
#include <stdlib.h>

#include "test_cmocka.h"

#include "sim_video_internal.h"

static void use_dummy_sdl_drivers(void)
{
#if defined(_WIN32)
    _putenv_s("SDL_AUDIODRIVER", "dummy");
    _putenv_s("SDL_VIDEODRIVER", "dummy");
#else
    setenv("SDL_AUDIODRIVER", "dummy", 1);
    setenv("SDL_VIDEODRIVER", "dummy", 1);
#endif
}

typedef struct {
    int init_subsystem_count;
    Uint32 init_subsystem_flags;
    int init_subsystem_result;
    int quit_subsystem_count;
    Uint32 quit_subsystem_flags;
    int joystick_event_state_count;
    int joystick_event_state_result;
    int game_controller_event_state_count;
    int game_controller_event_state_result;
    int num_joysticks_result;
    int num_joysticks_count;
    int is_game_controller_count;
    SDL_bool is_game_controller[4];
    int game_controller_open_count;
    int joystick_open_count;
    int game_controller_close_count;
    int joystick_close_count;
    SDL_GameControllerButtonBind button_bind;
} fake_sdl_state;

static fake_sdl_state fake_sdl;
static const fake_sdl_state default_fake_sdl = {
    .init_subsystem_result = 0,
    .joystick_event_state_result = SDL_ENABLE,
    .game_controller_event_state_result = SDL_ENABLE,
    .button_bind = {
        .bindType = SDL_CONTROLLER_BINDTYPE_BUTTON,
        .value.button = 9,
    },
};
static int fake_controller;
static int fake_joystick;
static int fake_controller_from_instance;

static void fake_get_version(SDL_version *ver)
{
    ver->major = 2;
    ver->minor = 0;
    ver->patch = 4;
}

static int fake_init_subsystem(Uint32 flags)
{
    fake_sdl.init_subsystem_count++;
    fake_sdl.init_subsystem_flags = flags;
    return fake_sdl.init_subsystem_result;
}

static void fake_quit_subsystem(Uint32 flags)
{
    fake_sdl.quit_subsystem_count++;
    fake_sdl.quit_subsystem_flags = flags;
}

static int fake_joystick_event_state(int state)
{
    assert_int_equal(state, SDL_ENABLE);
    fake_sdl.joystick_event_state_count++;
    return fake_sdl.joystick_event_state_result;
}

static int fake_game_controller_event_state(int state)
{
    assert_int_equal(state, SDL_ENABLE);
    fake_sdl.game_controller_event_state_count++;
    return fake_sdl.game_controller_event_state_result;
}

static int fake_num_joysticks(void)
{
    fake_sdl.num_joysticks_count++;
    return fake_sdl.num_joysticks_result;
}

static SDL_bool fake_is_game_controller(int joystick_index)
{
    assert_int_in_range(joystick_index, 0, 3);
    fake_sdl.is_game_controller_count++;
    return fake_sdl.is_game_controller[joystick_index];
}

static SDL_GameController *fake_game_controller_open(int joystick_index)
{
    assert_int_equal(joystick_index, 0);
    fake_sdl.game_controller_open_count++;
    return (SDL_GameController *)&fake_controller;
}

static void fake_game_controller_close(SDL_GameController *gamecontroller)
{
    assert_ptr_equal(gamecontroller, (SDL_GameController *)&fake_controller);
    fake_sdl.game_controller_close_count++;
}

static const char *fake_game_controller_name_for_index(int joystick_index)
{
    assert_int_equal(joystick_index, 0);
    return "controller";
}

static SDL_Joystick *fake_joystick_open(int device_index)
{
    assert_int_equal(device_index, 1);
    fake_sdl.joystick_open_count++;
    return (SDL_Joystick *)&fake_joystick;
}

static void fake_joystick_close(SDL_Joystick *joystick)
{
    assert_ptr_equal(joystick, (SDL_Joystick *)&fake_joystick);
    fake_sdl.joystick_close_count++;
}

static const char *fake_joystick_name_for_index(int device_index)
{
    assert_int_equal(device_index, 1);
    return "joystick";
}

static int fake_joystick_num_axes(SDL_Joystick *joystick)
{
    assert_ptr_equal(joystick, (SDL_Joystick *)&fake_joystick);
    return 2;
}

static int fake_joystick_num_buttons(SDL_Joystick *joystick)
{
    assert_ptr_equal(joystick, (SDL_Joystick *)&fake_joystick);
    return 3;
}

static SDL_GameController *fake_game_controller_from_instance_id(
    SDL_JoystickID joystick_id)
{
    assert_int_equal(joystick_id, 12);
    return (SDL_GameController *)&fake_controller_from_instance;
}

static SDL_GameControllerButtonBind fake_game_controller_get_bind_for_button(
    SDL_GameController *gamecontroller, SDL_GameControllerButton button)
{
    assert_ptr_equal(gamecontroller,
                     (SDL_GameController *)&fake_controller_from_instance);
    assert_int_equal(button, SDL_CONTROLLER_BUTTON_A);
    return fake_sdl.button_bind;
}

static const sim_video_sdl_hooks fake_sdl_hooks = {
    fake_get_version,
    fake_init_subsystem,
    fake_quit_subsystem,
    fake_joystick_event_state,
    fake_game_controller_event_state,
    fake_num_joysticks,
    fake_is_game_controller,
    fake_game_controller_open,
    fake_game_controller_close,
    fake_game_controller_name_for_index,
    fake_joystick_open,
    fake_joystick_close,
    fake_joystick_name_for_index,
    fake_joystick_num_axes,
    fake_joystick_num_buttons,
    fake_game_controller_from_instance_id,
    fake_game_controller_get_bind_for_button,
};

static void reset_fake_sdl_defaults(void)
{
    fake_sdl = default_fake_sdl;
}

static int setup_fake_sdl_hooks(void **state)
{
    (void)state;

    sim_video_reset_test_hooks();
    reset_fake_sdl_defaults();
    sim_video_set_test_sdl_hooks(&fake_sdl_hooks);
    return 0;
}

static void test_key_name_formats_known_and_unknown_keys(void **state)
{
    (void)state;

    assert_string_equal(vid_key_name(SIM_KEY_A), "SIM_KEY_A");
    assert_string_equal(vid_key_name(9999), "UNKNOWN KEY: 9999");
}

static void test_window_title_combines_simulator_and_window_title(void **state)
{
    char title[128];

    (void)state;

    sim_video_test_format_window_title(title, sizeof(title), NULL, "panel");
    assert_string_equal(title, "zimh-unit - panel");
}

static void test_screenshot_name_preserves_current_suffix_rules(void **state)
{
    char name[32];

    (void)state;

    sim_video_test_format_screenshot_name(name, sizeof(name), "screen.bmp", 0,
                                          false);
    assert_string_equal(name, "screen.bmp");

    sim_video_test_format_screenshot_name(name, sizeof(name), "screen.bmp", 2,
                                          true);
    assert_string_equal(name, "screen2.bmp");

    sim_video_test_format_screenshot_name(name, sizeof(name), "screen", 3,
                                          true);
    assert_string_equal(name, "screen3");
}

static void test_screenshot_reports_surface_allocation_failure(void **state)
{
    const char *path = "zimh-unit-video-screenshot.bmp";
    VID_DISPLAY *display = NULL;

    (void)state;

    use_dummy_sdl_drivers();
    remove(path);

    assert_int_equal(vid_open_window(&display, NULL, "screenshot", 16, 16, 0),
                     SCPE_OK);
    assert_non_null(display);

    assert_int_equal(vid_screenshot(path), SCPE_MEM);

    remove(path);
    assert_int_equal(vid_close_all(), SCPE_OK);
}

static void dummy_gamepad_callback(int control, int value, int state)
{
    (void)control;
    (void)value;
    (void)state;
}

static int teardown_gamepad_callbacks(void **state)
{
    (void)state;

    sim_video_reset_test_hooks();
    vid_active = 0;
    return 0;
}

/* Gamepad callbacks are optional and may be registered before video opens. */
static void test_gamepad_callbacks_register_before_open(void **state)
{
    (void)state;

    assert_int_equal(
        vid_register_gamepad_motion_callback(dummy_gamepad_callback),
        SCPE_OK);
    assert_int_equal(
        vid_register_gamepad_motion_callback(dummy_gamepad_callback),
        SCPE_ALATT);
    assert_int_equal(
        vid_register_gamepad_button_callback(dummy_gamepad_callback),
        SCPE_OK);
}

static void test_gamepad_callbacks_unregister(void **state)
{
    (void)state;

    assert_int_equal(
        vid_register_gamepad_motion_callback(dummy_gamepad_callback),
        SCPE_OK);
    assert_int_equal(
        vid_register_gamepad_button_callback(dummy_gamepad_callback),
        SCPE_OK);

    assert_int_equal(
        vid_unregister_gamepad_motion_callback(dummy_gamepad_callback),
        SCPE_OK);
    assert_int_equal(
        vid_unregister_gamepad_motion_callback(dummy_gamepad_callback),
        SCPE_NOATT);
    assert_int_equal(
        vid_unregister_gamepad_button_callback(dummy_gamepad_callback),
        SCPE_OK);
}

static void test_late_gamepad_registration_initializes_controllers_once(
    void **state)
{
    (void)state;

    vid_active = 1;

    assert_int_equal(
        vid_register_gamepad_motion_callback(dummy_gamepad_callback),
        SCPE_OK);
    assert_int_equal(fake_sdl.init_subsystem_count, 1);
    assert_int_equal(fake_sdl.init_subsystem_flags, SDL_INIT_GAMECONTROLLER);

    assert_int_equal(
        vid_register_gamepad_button_callback(dummy_gamepad_callback),
        SCPE_OK);
    assert_int_equal(fake_sdl.init_subsystem_count, 1);
}

static void test_gamepad_cleanup_closes_opened_handles(void **state)
{
    (void)state;

    fake_sdl.num_joysticks_result = 2;
    fake_sdl.is_game_controller[0] = SDL_TRUE;
    fake_sdl.is_game_controller[1] = SDL_FALSE;

    assert_int_equal(
        vid_register_gamepad_motion_callback(dummy_gamepad_callback),
        SCPE_OK);

    sim_video_test_setup_gamepad_controllers(NULL);

    assert_int_equal(fake_sdl.num_joysticks_count, 1);
    assert_int_equal(fake_sdl.is_game_controller_count, 2);
    assert_int_equal(fake_sdl.game_controller_open_count, 1);
    assert_int_equal(fake_sdl.joystick_open_count, 1);
    assert_int_equal(fake_sdl.game_controller_close_count, 0);
    assert_int_equal(fake_sdl.joystick_close_count, 0);

    sim_video_test_cleanup_gamepad_controllers();

    assert_int_equal(fake_sdl.game_controller_close_count, 1);
    assert_int_equal(fake_sdl.joystick_close_count, 1);
    assert_int_equal(fake_sdl.quit_subsystem_count, 1);
    assert_int_equal(fake_sdl.quit_subsystem_flags, SDL_INIT_GAMECONTROLLER);
}

static void test_unregistering_last_callback_cleans_up_gamepad_handles(
    void **state)
{
    (void)state;

    fake_sdl.num_joysticks_result = 1;
    fake_sdl.is_game_controller[0] = SDL_TRUE;

    assert_int_equal(
        vid_register_gamepad_motion_callback(dummy_gamepad_callback),
        SCPE_OK);

    sim_video_test_setup_gamepad_controllers(NULL);

    assert_int_equal(fake_sdl.game_controller_open_count, 1);
    assert_int_equal(fake_sdl.game_controller_close_count, 0);

    assert_int_equal(
        vid_unregister_gamepad_motion_callback(dummy_gamepad_callback),
        SCPE_OK);

    assert_int_equal(fake_sdl.game_controller_close_count, 1);
    assert_int_equal(fake_sdl.quit_subsystem_count, 1);
    assert_int_equal(fake_sdl.quit_subsystem_flags, SDL_INIT_GAMECONTROLLER);
}

static void test_gamepad_setup_failure_resets_initialization(void **state)
{
    (void)state;

    fake_sdl.joystick_event_state_result = -1;

    assert_int_equal(
        vid_register_gamepad_motion_callback(dummy_gamepad_callback),
        SCPE_OK);

    sim_video_test_setup_gamepad_controllers(NULL);

    assert_int_equal(fake_sdl.init_subsystem_count, 1);
    assert_int_equal(fake_sdl.quit_subsystem_count, 1);
    assert_int_equal(fake_sdl.quit_subsystem_flags, SDL_INIT_GAMECONTROLLER);

    fake_sdl.joystick_event_state_result = SDL_ENABLE;
    sim_video_test_setup_gamepad_controllers(NULL);

    assert_int_equal(fake_sdl.init_subsystem_count, 2);
    assert_int_equal(fake_sdl.num_joysticks_count, 1);
}

static void test_gamepad_init_failure_resets_initialization(void **state)
{
    (void)state;

    fake_sdl.init_subsystem_result = -1;

    assert_int_equal(
        vid_register_gamepad_motion_callback(dummy_gamepad_callback),
        SCPE_OK);

    sim_video_test_setup_gamepad_controllers(NULL);

    assert_int_equal(fake_sdl.init_subsystem_count, 1);
    assert_int_equal(fake_sdl.joystick_event_state_count, 0);
    assert_int_equal(fake_sdl.quit_subsystem_count, 0);

    fake_sdl.init_subsystem_result = 0;
    sim_video_test_setup_gamepad_controllers(NULL);

    assert_int_equal(fake_sdl.init_subsystem_count, 2);
    assert_int_equal(fake_sdl.joystick_event_state_count, 1);
}

typedef struct {
    int count;
    int device;
    int control;
    int value;
} callback_event;

static callback_event motion_event;
static callback_event button_event;

static void record_motion_event(int device, int control, int value)
{
    motion_event.count++;
    motion_event.device = device;
    motion_event.control = control;
    motion_event.value = value;
}

static void record_button_event(int device, int control, int value)
{
    button_event.count++;
    button_event.device = device;
    button_event.control = control;
    button_event.value = value;
}

static void test_gamepad_events_reach_registered_callbacks(void **state)
{
    SDL_JoyAxisEvent joy_axis_event = {0};
    SDL_JoyButtonEvent joy_button_event = {0};
    SDL_ControllerAxisEvent controller_axis_event = {0};
    SDL_ControllerButtonEvent controller_button_event = {0};

    (void)state;

    motion_event = (callback_event){0};
    button_event = (callback_event){0};

    assert_int_equal(vid_register_gamepad_motion_callback(record_motion_event),
                     SCPE_OK);
    assert_int_equal(vid_register_gamepad_button_callback(record_button_event),
                     SCPE_OK);

    joy_axis_event.which = 7;
    joy_axis_event.axis = 2;
    joy_axis_event.value = -123;
    vid_joy_motion(&joy_axis_event);

    assert_int_equal(motion_event.count, 1);
    assert_int_equal(motion_event.device, 7);
    assert_int_equal(motion_event.control, 2);
    assert_int_equal(motion_event.value, -123);

    controller_axis_event.which = 8;
    controller_axis_event.axis = 3;
    controller_axis_event.value = 456;
    vid_controller_motion(&controller_axis_event);

    assert_int_equal(motion_event.count, 2);
    assert_int_equal(motion_event.device, 8);
    assert_int_equal(motion_event.control, 3);
    assert_int_equal(motion_event.value, 456);

    joy_button_event.which = 11;
    joy_button_event.button = 4;
    joy_button_event.state = SDL_PRESSED;
    vid_joy_button(&joy_button_event);

    assert_int_equal(button_event.count, 1);
    assert_int_equal(button_event.device, 11);
    assert_int_equal(button_event.control, 4);
    assert_int_equal(button_event.value, SDL_PRESSED);

#if (SDL_MAJOR_VERSION > 2) || (SDL_MAJOR_VERSION == 2 && \
    (SDL_MINOR_VERSION > 0) || (SDL_PATCHLEVEL >= 4))
    controller_button_event.which = 12;
    controller_button_event.button = SDL_CONTROLLER_BUTTON_A;
    controller_button_event.state = SDL_RELEASED;
    vid_controller_button(&controller_button_event);

    assert_int_equal(button_event.count, 2);
    assert_int_equal(button_event.device, 12);
    assert_int_equal(button_event.control, 9);
    assert_int_equal(button_event.value, SDL_RELEASED);
#endif
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_key_name_formats_known_and_unknown_keys),
        cmocka_unit_test(test_window_title_combines_simulator_and_window_title),
        cmocka_unit_test(test_screenshot_name_preserves_current_suffix_rules),
        cmocka_unit_test(test_screenshot_reports_surface_allocation_failure),
        cmocka_unit_test_teardown(test_gamepad_callbacks_register_before_open,
                                  teardown_gamepad_callbacks),
        cmocka_unit_test_teardown(test_gamepad_callbacks_unregister,
                                  teardown_gamepad_callbacks),
        cmocka_unit_test_setup_teardown(
            test_late_gamepad_registration_initializes_controllers_once,
            setup_fake_sdl_hooks, teardown_gamepad_callbacks),
        cmocka_unit_test_setup_teardown(
            test_gamepad_cleanup_closes_opened_handles, setup_fake_sdl_hooks,
            teardown_gamepad_callbacks),
        cmocka_unit_test_setup_teardown(
            test_unregistering_last_callback_cleans_up_gamepad_handles,
            setup_fake_sdl_hooks, teardown_gamepad_callbacks),
        cmocka_unit_test_setup_teardown(
            test_gamepad_setup_failure_resets_initialization,
            setup_fake_sdl_hooks, teardown_gamepad_callbacks),
        cmocka_unit_test_setup_teardown(
            test_gamepad_init_failure_resets_initialization,
            setup_fake_sdl_hooks, teardown_gamepad_callbacks),
        cmocka_unit_test_setup_teardown(
            test_gamepad_events_reach_registered_callbacks,
            setup_fake_sdl_hooks, teardown_gamepad_callbacks),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
