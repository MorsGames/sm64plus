#ifndef GFX_WINDOW_MANAGER_API_H
#define GFX_WINDOW_MANAGER_API_H

#include <stdint.h>
#include <stdbool.h>
#include <PR/ultratypes.h>

struct GfxWindowManagerAPI {
    void (*init)(const char *game_name, bool start_in_fullscreen);
    void (*set_keyboard_callbacks)(bool (*on_key_down)(int scancode), bool (*on_key_up)(int scancode), void (*on_all_keys_up)(void), void (*on_mouse_move)(long x, long y), void (*on_mouse_press)(s8 left, s8 right, s8 middle, s8 wheel));
    void (*set_fullscreen_changed_callback)(void (*on_fullscreen_changed)(bool is_now_fullscreen));
    void (*set_fullscreen)(bool enable);
    void (*set_vsync)(bool enable);
    void (*set_window_size)(uint32_t width, uint32_t height);
    void (*set_monitor)(uint32_t monitor_index);
    int (*get_num_display_modes)(void);
    void (*main_loop)(void (*run_one_game_iter)(void));
    void (*get_dimensions)(uint32_t *width, uint32_t *height);
    void (*handle_events)(void);
    bool (*start_frame)(void);
    void (*swap_buffers_begin)(void);
    void (*swap_buffers_end)(void);
    double (*get_time)(void); // For debug
};

#endif
