#include "../compat.h"

#ifdef __MINGW32__
#define FOR_WINDOWS 1
#else
#define FOR_WINDOWS 0
#endif

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#ifdef VERSION_EU
# define FRAME_RATE 50
#else
# define FRAME_RATE 60
#endif

#if FOR_WINDOWS
#include <GL/glew.h>
#include "SDL.h"
#include "SDL_syswm.h"
#define GL_GLEXT_PROTOTYPES 1
#include "SDL_opengl.h"
#else
#ifndef TARGET_MACOS
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#else
#include <SDL_opengl.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <stdio.h>
#endif
#define GL_GLEXT_PROTOTYPES 1
#ifndef TARGET_MACOS
#include <SDL2/SDL_opengles2.h>
#endif
#endif

#include "gfx_window_manager_api.h"
#include "gfx_screen_config.h"

#include "game/settings.h"

#if defined(_WIN32) || defined(_WIN64)
#include "../../include/resource.h"
#endif

#define GFX_API_NAME "OpenGL"

static SDL_Window *wnd;
static int inverted_scancode_table[512];

static unsigned int window_width;
static unsigned int window_height;
static bool fullscreen_state;
static void (*on_fullscreen_changed_callback)(bool is_now_fullscreen);
static bool (*on_key_down_callback)(int scancode);
static bool (*on_key_up_callback)(int scancode);
static void (*on_all_keys_up_callback)(void);
static void (*on_mouse_move_callback)(long x, long y);
static void (*on_mouse_press_callback)(s8 left, s8 right, s8 middle, s8 wheel);

static bool relative_mouse_mode_on = false;

static Uint64 min_ticks_per_frame = 0;
static Uint64 next_frame_ticks = 0;
static Uint64 perf_freq = 0;

static int8_t clamp_s8(long val) {
    if (val < -128) return -128;
    if (val > 127) return 127;
    return val;
}

const SDL_Scancode windows_scancode_table[] =
{ 
    /*	0						1							2							3							4						5							6							7 */
    /*	8						9							A							B							C						D							E							F */
    SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_ESCAPE,		SDL_SCANCODE_1,				SDL_SCANCODE_2,				SDL_SCANCODE_3,			SDL_SCANCODE_4,				SDL_SCANCODE_5,				SDL_SCANCODE_6,			/* 0 */
    SDL_SCANCODE_7,				SDL_SCANCODE_8,				SDL_SCANCODE_9,				SDL_SCANCODE_0,				SDL_SCANCODE_MINUS,		SDL_SCANCODE_EQUALS,		SDL_SCANCODE_BACKSPACE,		SDL_SCANCODE_TAB,		/* 0 */

    SDL_SCANCODE_Q,				SDL_SCANCODE_W,				SDL_SCANCODE_E,				SDL_SCANCODE_R,				SDL_SCANCODE_T,			SDL_SCANCODE_Y,				SDL_SCANCODE_U,				SDL_SCANCODE_I,			/* 1 */
    SDL_SCANCODE_O,				SDL_SCANCODE_P,				SDL_SCANCODE_LEFTBRACKET,	SDL_SCANCODE_RIGHTBRACKET,	SDL_SCANCODE_RETURN,	SDL_SCANCODE_LCTRL,			SDL_SCANCODE_A,				SDL_SCANCODE_S,			/* 1 */

    SDL_SCANCODE_D,				SDL_SCANCODE_F,				SDL_SCANCODE_G,				SDL_SCANCODE_H,				SDL_SCANCODE_J,			SDL_SCANCODE_K,				SDL_SCANCODE_L,				SDL_SCANCODE_SEMICOLON,	/* 2 */
    SDL_SCANCODE_APOSTROPHE,	SDL_SCANCODE_GRAVE,			SDL_SCANCODE_LSHIFT,		SDL_SCANCODE_BACKSLASH,		SDL_SCANCODE_Z,			SDL_SCANCODE_X,				SDL_SCANCODE_C,				SDL_SCANCODE_V,			/* 2 */

    SDL_SCANCODE_B,				SDL_SCANCODE_N,				SDL_SCANCODE_M,				SDL_SCANCODE_COMMA,			SDL_SCANCODE_PERIOD,	SDL_SCANCODE_SLASH,			SDL_SCANCODE_RSHIFT,		SDL_SCANCODE_PRINTSCREEN,/* 3 */
    SDL_SCANCODE_LALT,			SDL_SCANCODE_SPACE,			SDL_SCANCODE_CAPSLOCK,		SDL_SCANCODE_F1,			SDL_SCANCODE_F2,		SDL_SCANCODE_F3,			SDL_SCANCODE_F4,			SDL_SCANCODE_F5,		/* 3 */

    SDL_SCANCODE_F6,			SDL_SCANCODE_F7,			SDL_SCANCODE_F8,			SDL_SCANCODE_F9,			SDL_SCANCODE_F10,		SDL_SCANCODE_NUMLOCKCLEAR,	SDL_SCANCODE_SCROLLLOCK,	SDL_SCANCODE_HOME,		/* 4 */
    SDL_SCANCODE_UP,			SDL_SCANCODE_PAGEUP,		SDL_SCANCODE_KP_MINUS,		SDL_SCANCODE_LEFT,			SDL_SCANCODE_KP_5,		SDL_SCANCODE_RIGHT,			SDL_SCANCODE_KP_PLUS,		SDL_SCANCODE_END,		/* 4 */

    SDL_SCANCODE_DOWN,			SDL_SCANCODE_PAGEDOWN,		SDL_SCANCODE_INSERT,		SDL_SCANCODE_DELETE,		SDL_SCANCODE_UNKNOWN,	SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_NONUSBACKSLASH,SDL_SCANCODE_F11,		/* 5 */
    SDL_SCANCODE_F12,			SDL_SCANCODE_PAUSE,			SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_LGUI,			SDL_SCANCODE_RGUI,		SDL_SCANCODE_APPLICATION,	SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,	/* 5 */

    SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_F13,		SDL_SCANCODE_F14,			SDL_SCANCODE_F15,			SDL_SCANCODE_F16,		/* 6 */
    SDL_SCANCODE_F17,			SDL_SCANCODE_F18,			SDL_SCANCODE_F19,			SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,	SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,	/* 6 */

    SDL_SCANCODE_INTERNATIONAL2,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_INTERNATIONAL1,		SDL_SCANCODE_UNKNOWN,	SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,	/* 7 */
    SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_INTERNATIONAL4,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_INTERNATIONAL5,		SDL_SCANCODE_UNKNOWN,	SDL_SCANCODE_INTERNATIONAL3,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN	/* 7 */
};

const SDL_Scancode scancode_rmapping_extended[][2] = {
    {SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_RETURN},
    {SDL_SCANCODE_RALT, SDL_SCANCODE_LALT},
    {SDL_SCANCODE_RCTRL, SDL_SCANCODE_LCTRL},
    {SDL_SCANCODE_KP_DIVIDE, SDL_SCANCODE_SLASH},
    //{SDL_SCANCODE_KP_PLUS, SDL_SCANCODE_CAPSLOCK}
};

const SDL_Scancode scancode_rmapping_nonextended[][2] = {
    {SDL_SCANCODE_KP_7, SDL_SCANCODE_HOME},
    {SDL_SCANCODE_KP_8, SDL_SCANCODE_UP},
    {SDL_SCANCODE_KP_9, SDL_SCANCODE_PAGEUP},
    {SDL_SCANCODE_KP_4, SDL_SCANCODE_LEFT},
    {SDL_SCANCODE_KP_6, SDL_SCANCODE_RIGHT},
    {SDL_SCANCODE_KP_1, SDL_SCANCODE_END},
    {SDL_SCANCODE_KP_2, SDL_SCANCODE_DOWN},
    {SDL_SCANCODE_KP_3, SDL_SCANCODE_PAGEDOWN},
    {SDL_SCANCODE_KP_0, SDL_SCANCODE_INSERT},
    {SDL_SCANCODE_KP_PERIOD, SDL_SCANCODE_DELETE},
    {SDL_SCANCODE_KP_MULTIPLY, SDL_SCANCODE_PRINTSCREEN}
};

static void set_fullscreen(bool on, bool call_callback) {
    if (fullscreen_state == on) {
        return;
    }
    fullscreen_state = on;

    if (on) {
        SDL_DisplayMode mode;
        if (configCustomFullscreenResolution) {
            mode.format = SDL_PIXELFORMAT_ARGB8888;
            mode.w = configFullscreenWidth;
            mode.h = configFullscreenHeight;
            mode.refresh_rate = configFullscreenRefreshRate;
            mode.driverdata = 0;
            SDL_SetWindowDisplayMode(wnd, &mode);
        }
        else {
            SDL_GetDesktopDisplayMode(0, &mode);
        }
        SDL_SetWindowSize(wnd, mode.w, mode.h);
        SDL_SetWindowFullscreen(wnd, configCustomFullscreenResolution ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_ShowCursor(false);
    } else {
        if (configCustomFullscreenResolution) {
            SDL_DisplayMode mode;
            SDL_GetDesktopDisplayMode(0, &mode);
            SDL_SetWindowDisplayMode(wnd, &mode);
        }
        SDL_SetWindowFullscreen(wnd, 0);
        SDL_SetWindowSize(wnd, window_width, window_height);
        SDL_ShowCursor(true);
    }

    if (on_fullscreen_changed_callback != NULL && call_callback) {
        on_fullscreen_changed_callback(on);
    }
}

static void gfx_sdl_init(const char *game_name, bool start_in_fullscreen) {
    window_width = DESIRED_SCREEN_WIDTH;
    window_height = DESIRED_SCREEN_HEIGHT;

    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");

    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    wnd = SDL_CreateWindow(game_name,
            SDL_WINDOWPOS_UNDEFINED_DISPLAY(configDefaultMonitor-1),
            SDL_WINDOWPOS_UNDEFINED_DISPLAY(configDefaultMonitor-1),
            window_width, window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);

#if defined(_WIN32) || defined(_WIN64)
    // Set window icon from embedded resources
    HINSTANCE hInstance = GetModuleHandle(NULL);
    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(STAR_ICON));
    if (hIcon) {
        // Get the window handle from SDL
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (SDL_GetWindowWMInfo(wnd, &wmInfo)) {
            HWND hwnd = wmInfo.info.win.window;
            // Set both the large and small icons
            SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        }
    }
#endif

    if (start_in_fullscreen) {
        set_fullscreen(true, false);
    }

    if (configMouseCam) {
        SDL_SetRelativeMouseMode(true);
        relative_mouse_mode_on = true;
    }

    SDL_GL_CreateContext(wnd);

    SDL_GL_SetSwapInterval(configVSync ? 1 : 0);

    SDL_DisableScreenSaver();
    
    perf_freq = SDL_GetPerformanceFrequency();
    min_ticks_per_frame = perf_freq / FRAME_RATE;
    next_frame_ticks = SDL_GetPerformanceCounter() + min_ticks_per_frame;
    
    for (size_t i = 0; i < sizeof(windows_scancode_table) / sizeof(SDL_Scancode); i++) {
        inverted_scancode_table[windows_scancode_table[i]] = i;
    }

    for (size_t i = 0; i < sizeof(scancode_rmapping_extended) / sizeof(scancode_rmapping_extended[0]); i++) {
        inverted_scancode_table[scancode_rmapping_extended[i][0]] = inverted_scancode_table[scancode_rmapping_extended[i][1]] + 0x100;
    }

    for (size_t i = 0; i < sizeof(scancode_rmapping_nonextended) / sizeof(scancode_rmapping_nonextended[0]); i++) {
        inverted_scancode_table[scancode_rmapping_nonextended[i][0]] = inverted_scancode_table[scancode_rmapping_nonextended[i][1]];
        inverted_scancode_table[scancode_rmapping_nonextended[i][1]] += 0x100;
    }
}

static void gfx_sdl_set_fullscreen_changed_callback(void (*on_fullscreen_changed)(bool is_now_fullscreen)) {
    on_fullscreen_changed_callback = on_fullscreen_changed;
}

static void gfx_sdl_set_fullscreen(bool enable) {
    set_fullscreen(enable, true);
}

static void gfx_sdl_set_keyboard_callbacks(bool (*on_key_down)(int scancode), bool (*on_key_up)(int scancode), void (*on_all_keys_up)(void), void (*on_mouse_move)(long x, long y), void (*on_mouse_press)(s8 left, s8 right, s8 middle, s8 wheel)) {
    on_key_down_callback = on_key_down;
    on_key_up_callback = on_key_up;
    on_all_keys_up_callback = on_all_keys_up;
    on_mouse_move_callback = on_mouse_move;
    on_mouse_press_callback = on_mouse_press;
}

static void gfx_sdl_main_loop(void (*run_one_game_iter)(void)) {
    while (1) {
        run_one_game_iter();
        if (configMouseCam) {
            bool want = (SDL_GetWindowFlags(wnd) & SDL_WINDOW_INPUT_FOCUS) == SDL_WINDOW_INPUT_FOCUS;
            if (want != relative_mouse_mode_on) {
                SDL_SetRelativeMouseMode(want);
                relative_mouse_mode_on = want;
            }
        } else if (relative_mouse_mode_on) {
            SDL_SetRelativeMouseMode(false);
            relative_mouse_mode_on = false;
        }
    }
}

static void gfx_sdl_get_dimensions(uint32_t *width, uint32_t *height) {
    int w, h;
    SDL_GL_GetDrawableSize(wnd, &w, &h);
    *width = w;
    *height = h;
}

static int translate_scancode(int scancode) {
    if (scancode < 512) {
        return inverted_scancode_table[scancode];
    } else {
        return 0;
    }
}

static void gfx_sdl_onkeydown(int scancode) {
    int key = translate_scancode(scancode);
    if (on_key_down_callback != NULL) {
        on_key_down_callback(key);
    }
}

static void gfx_sdl_onkeyup(int scancode) {
    int key = translate_scancode(scancode);
    if (on_key_up_callback != NULL) {
        on_key_up_callback(key);
    }
}

static void gfx_sdl_handle_events(void) {
    SDL_Event event;
    Uint8 *state = SDL_GetKeyboardState(NULL);
    on_mouse_move_callback(0, 0);
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
#ifndef TARGET_WEB
            // Scancodes are broken in Emscripten SDL2: https://bugzilla.libsdl.org/show_bug.cgi?id=3259
            case SDL_KEYDOWN:
                if (state[SDL_SCANCODE_RETURN] && state[SDL_SCANCODE_LALT]) {
                    set_fullscreen(!fullscreen_state, true);
                    break;
                }
                
                gfx_sdl_onkeydown(event.key.keysym.scancode);
                break;
            case SDL_KEYUP:
                gfx_sdl_onkeyup(event.key.keysym.scancode);
                break;
#endif
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED && (SDL_GetWindowFlags(SDL_GetWindowFromID(event.window.windowID)) & SDL_WINDOW_FULLSCREEN) == 0) {
                    window_width = event.window.data1;
                    window_height = event.window.data2;
                }
                break;

            case SDL_QUIT:
                exit(0);
                break;

            case SDL_MOUSEMOTION:
                on_mouse_move_callback(event.motion.xrel*10, event.motion.yrel*10);
                break;

            case SDL_MOUSEBUTTONDOWN:
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT:
                        on_mouse_press_callback(1, 0, 0, 0);
                        break;
                    case SDL_BUTTON_RIGHT:
                        on_mouse_press_callback(0, 1, 0, 0);
                        break;
                    case SDL_BUTTON_MIDDLE:
                        on_mouse_press_callback(0, 0, 1, 0);
                        break;
                }
                break;

            case SDL_MOUSEBUTTONUP:
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT:
                        on_mouse_press_callback(-1, 0, 0, 0);
                        break;
                    case SDL_BUTTON_RIGHT:
                        on_mouse_press_callback(0, -1, 0, 0);
                        break;
                    case SDL_BUTTON_MIDDLE:
                        on_mouse_press_callback(0, 0, -1, 0);
                        break;
                }
                break;

            case SDL_MOUSEWHEEL:
                on_mouse_press_callback(0, 0, 0, clamp_s8(event.wheel.y));
                break;
        }
    }
}

static bool gfx_sdl_start_frame(void) {
    return true;
}

static void gfx_sdl_swap_buffers_begin(void) {
    SDL_GL_SwapWindow(wnd);
}

static void gfx_sdl_swap_buffers_end(void) {
    // Get the current time in ticks
    Uint64 now_ticks = SDL_GetPerformanceCounter();

    // If we're ahead of schedule, wait until it's time for the next frame
    if (now_ticks < next_frame_ticks) {
        const Uint64 ticks_remaining = next_frame_ticks - now_ticks;
        const Uint64 ms_remaining = (ticks_remaining * 1000) / perf_freq;

        // Sleep for most of the remaining time
        if (ms_remaining >= 2) {
            Uint32 coarse_ms = (Uint32)(ms_remaining - 1);
            SDL_Delay(coarse_ms);
        }

        // Busy-wait for the rest
        while (true) {
            now_ticks = SDL_GetPerformanceCounter();
            if (now_ticks >= next_frame_ticks)
                 break;
            SDL_Delay(0);
        }
    }

    // If we're running behind, then FUCK!!!
    now_ticks = SDL_GetPerformanceCounter();
    if (now_ticks >= next_frame_ticks) {
        next_frame_ticks = now_ticks + min_ticks_per_frame;
    }
}

static double gfx_sdl_get_time(void) {
    return 0.0;
}

struct GfxWindowManagerAPI gfx_sdl = {
    gfx_sdl_init,
    gfx_sdl_set_keyboard_callbacks,
    gfx_sdl_set_fullscreen_changed_callback,
    gfx_sdl_set_fullscreen,
    gfx_sdl_main_loop,
    gfx_sdl_get_dimensions,
    gfx_sdl_handle_events,
    gfx_sdl_start_frame,
    gfx_sdl_swap_buffers_begin,
    gfx_sdl_swap_buffers_end,
    gfx_sdl_get_time
};