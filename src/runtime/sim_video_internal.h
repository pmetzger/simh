/* sim_video_internal.h: Internal video interfaces for tests */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: X11

#ifndef SIM_VIDEO_INTERNAL_H_
#define SIM_VIDEO_INTERNAL_H_ 1

#include "sim_video.h"

/* Clear test-installed gamepad callbacks between unit tests. */
void sim_video_reset_test_gamepad_callbacks(void);

#if defined(USE_SIM_VIDEO) && defined(HAVE_LIBSDL)
#include <SDL.h>

typedef struct {
    void (*get_version)(SDL_version *ver);
    int (*init_subsystem)(Uint32 flags);
    void (*quit_subsystem)(Uint32 flags);
    int (*joystick_event_state)(int state);
    int (*game_controller_event_state)(int state);
    int (*num_joysticks)(void);
    SDL_bool (*is_game_controller)(int joystick_index);
    SDL_GameController *(*game_controller_open)(int joystick_index);
    void (*game_controller_close)(SDL_GameController *gamecontroller);
    const char *(*game_controller_name_for_index)(int joystick_index);
    SDL_Joystick *(*joystick_open)(int device_index);
    void (*joystick_close)(SDL_Joystick *joystick);
    const char *(*joystick_name_for_index)(int device_index);
    int (*joystick_num_axes)(SDL_Joystick *joystick);
    int (*joystick_num_buttons)(SDL_Joystick *joystick);
    SDL_GameController *(*game_controller_from_instance_id)(
        SDL_JoystickID joystick_id);
    SDL_GameControllerButtonBind (*game_controller_get_bind_for_button)(
        SDL_GameController *gamecontroller, SDL_GameControllerButton button);
    SDL_Surface *(*create_rgb_surface)(Uint32 flags, int width, int height,
                                       int depth, Uint32 rmask, Uint32 gmask,
                                       Uint32 bmask, Uint32 amask);
} sim_video_sdl_hooks;

/* Install SDL hooks for deterministic video gamepad unit tests. */
void sim_video_set_test_sdl_hooks(const sim_video_sdl_hooks *hooks);

/* Restore default SDL hooks and clear any test-owned controller state. */
void sim_video_reset_test_hooks(void);

/* Exercise video string-formatting helpers without opening an SDL window. */
void sim_video_test_format_window_title(char *title_buf, size_t title_size,
                                        DEVICE *dptr, const char *title);
void sim_video_test_format_screenshot_name(char *name, size_t name_size,
                                           const char *filename, int index,
                                           bool multiple);
/* Exercise screenshot logic directly without posting an SDL event. */
t_stat sim_video_test_screenshot_displays(const char *filename);
/* Exercise the screenshot event handler without the SDL event queue. */
t_stat sim_video_test_screenshot_event(const char *filename);

/* Exercise controller setup and cleanup without opening a real video window. */
void sim_video_test_setup_gamepad_controllers(DEVICE *dev);
void sim_video_test_cleanup_gamepad_controllers(void);

void vid_joy_motion(SDL_JoyAxisEvent *event);
void vid_joy_button(SDL_JoyButtonEvent *event);
void vid_controller_motion(SDL_ControllerAxisEvent *event);
void vid_controller_button(SDL_ControllerButtonEvent *event);
#endif

#endif
