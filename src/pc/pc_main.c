#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <pwd.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <dirent.h>
#endif

#ifdef TARGET_WEB
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "sm64.h"

#include "game/memory.h"
#include "audio/external.h"

#include "gfx/gfx_pc.h"
#include "gfx/gfx_opengl.h"
#include "gfx/gfx_sdl.h"
#include "gfx/gfx_dummy.h"

#include "audio/audio_api.h"
#include "audio/audio_sdl.h"
#include "audio/audio_null.h"

#include "controller/controller_keyboard.h"

#include "game/hud.h"
#include "configfile.h"

#include "game/rumble_init.h"
#include "game/settings.h"
#include "colors.h"

#include "compat.h"

#define CONFIG_FILE "settings.ini"

OSMesg gMainReceivedMesg;
OSMesgQueue gSIEventMesgQueue;

s8 gResetTimer;
s8 gNmiResetBarsTimer;
s8 gDebugLevelSelect;
s8 gShowProfiler;
s8 gShowDebugText;

static struct AudioAPI *audio_api;
static struct GfxWindowManagerAPI *wm_api;
static struct GfxRenderingAPI *rendering_api;

extern void gfx_run(Gfx *commands);
extern void thread5_game_loop(void *arg);
extern void create_next_audio_buffer(s16 *samples, u32 num_samples);
void game_loop_one_iteration(void);

void dispatch_audio_sptask(UNUSED struct SPTask *spTask) {
}

void set_vblank_handler(UNUSED s32 index, UNUSED struct VblankHandler *handler, UNUSED OSMesgQueue *queue, UNUSED OSMesg *msg) {
}

static uint8_t inited = 0;

#include "game/game_init.h" // for gGlobalTimer
void exec_display_list(struct SPTask *spTask) {
    if (!inited) {
        return;
    }
    gfx_run((Gfx *)spTask->task.t.data_ptr);
}

#define printf

#ifdef VERSION_EU
#define SAMPLES_HIGH 656
#define SAMPLES_LOW 640
#else
#define SAMPLES_HIGH 544
#define SAMPLES_LOW 528
#endif

static void patch_interpolations(void) {
    extern void mtx_patch_interpolated(void);
    extern void patch_screen_transition_interpolated(void);
    extern void patch_title_screen_scales(void);
    extern void patch_interpolated_dialog(void);
    extern void patch_interpolated_hud(void);
    extern void patch_interpolated_paintings(void);
    extern void patch_interpolated_bubble_particles(void);
    extern void patch_interpolated_snow_particles(void);
    mtx_patch_interpolated();
    patch_screen_transition_interpolated();
    patch_title_screen_scales();
    patch_interpolated_dialog();
    patch_interpolated_hud();

    if (configStayInCourse)
        render_you_got_a_star(1);

    patch_interpolated_paintings();
    patch_interpolated_bubble_particles();
    patch_interpolated_snow_particles();
}

void produce_one_frame(void) {
    gfx_start_frame();
    game_loop_one_iteration();

#if ENABLE_RUMBLE
    thread6_rumble_loop(NULL);
#endif
    
    int samples_left = audio_api->buffered();
    u32 num_audio_samples = samples_left < audio_api->get_desired_buffered() ? SAMPLES_HIGH : SAMPLES_LOW;
    //printf("Audio samples: %d %u\n", samples_left, num_audio_samples);
    s16 audio_buffer[SAMPLES_HIGH * 2 * 2];
    for (int i = 0; i < 2; i++) {
        /*if (audio_cnt-- == 0) {
            audio_cnt = 2;
        }
        u32 num_audio_samples = audio_cnt < 2 ? 528 : 544;*/
        create_next_audio_buffer(audio_buffer + i * (num_audio_samples * 2), num_audio_samples);
    }
    //printf("Audio samples before submitting: %d\n", audio_api->buffered());
    audio_api->play((u8 *)audio_buffer, 2 * num_audio_samples * 4);
    
    gfx_end_frame();
    
    gfx_start_frame();
    if (configFrameRate) {
        patch_interpolations();
    }
    else if (configStayInCourse) {
        render_you_got_a_star(3);
    }
    exec_display_list(gGfxSPTask);
    gfx_end_frame();
}

#ifdef TARGET_WEB
static void em_main_loop(void) {
}

static void request_anim_frame(void (*func)(double time)) {
    EM_ASM(requestAnimationFrame(function(time) {
        dynCall("vd", $0, [time]);
    }), func);
}

static void on_anim_frame(double time) {
    static double target_time;

    time *= 0.03; // milliseconds to frame count (33.333 ms -> 1)

    if (time >= target_time + 10.0) {
        // We are lagging 10 frames behind, probably due to coming back after inactivity,
        // so reset, with a small margin to avoid potential jitter later.
        target_time = time - 0.010;
    }

    for (int i = 0; i < 2; i++) {
        // If refresh rate is 15 Hz or something we might need to generate two frames
        if (time >= target_time) {
            produce_one_frame();
            target_time = target_time + 1.0;
        }
    }

    request_anim_frame(on_anim_frame);
}
#endif

static void save_config(void) {
    configfile_save(CONFIG_FILE);
}

static void on_fullscreen_changed(bool is_now_fullscreen) {
    configFullscreen = is_now_fullscreen;
}

// used primarily in gfx_pc.c
const char* GFX_DIR_PATH = NULL;

void main_func(const char* gfx_dir) {
    GFX_DIR_PATH = gfx_dir;
#ifdef USE_SYSTEM_MALLOC
    main_pool_init();
    gGfxAllocOnlyPool = alloc_only_pool_init();
#else
    static u64 pool[0x165000/8 / 4 * sizeof(void *)];
    main_pool_init(pool, pool + sizeof(pool) / sizeof(pool[0]));
#endif
    gEffectsMemoryPool = mem_pool_init(0x4000, MEMORY_POOL_LEFT);

#if defined(_WIN32) || defined(_WIN64)
    // Set the working directory
    char *workingdir = malloc(128);
    strcpy(workingdir, getenv("LOCALAPPDATA"));
    strcat(workingdir, "\\SM64Plus\\");
    chdir(workingdir);
#endif
#ifdef TARGET_LINUX
    // Set the working directory
    char *workingdir = malloc(128);
    const char *homedir;
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
    strcpy(workingdir, homedir);
    strcat(workingdir, "/.config/SM64Plus/");
    chdir(workingdir);
#endif

    // Check if the textures exist
#if defined(CUSTOM_TEXTURES) && (defined(_WIN32) || defined(_WIN64))
    DIR* dir = opendir("gfx");
    if (dir) {
        closedir(dir);
    } 
    else {
        MessageBox(0, "Failed to find the \"gfx\" folder in \"%LOCALAPPDATA%\\SM64Plus\". If you're using the launcher, try reinstalling the game. If not, copy the folder from \"build\\us_pc\".", "ERROR", MB_ICONERROR);
        exit(1);
    }

#endif

    configfile_load(CONFIG_FILE);

    // Set the custom colors
    set_colors();

    atexit(save_config);

#ifdef TARGET_WEB
    emscripten_set_main_loop(em_main_loop, 0, 0);
    request_anim_frame(on_anim_frame);
#endif

    switch (configGraphicsBackend)
    {
    case -1:
        rendering_api = &gfx_dummy_renderer_api;
        wm_api = &gfx_dummy_wm_api;
        break;
    default:
        rendering_api = &gfx_opengl_api;
        wm_api = &gfx_sdl;
        break;
    }
    gfx_init(wm_api, rendering_api, gTitleString, configFullscreen);
    
    wm_api->set_fullscreen_changed_callback(on_fullscreen_changed);
    wm_api->set_keyboard_callbacks(keyboard_on_key_down, keyboard_on_key_up, keyboard_on_all_keys_up, keyboard_on_mouse_move, keyboard_on_mouse_press);
    
    if (audio_sdl.init()) {
        audio_api = &audio_sdl;
    }
    else {
        audio_api = &audio_null;
    }

    audio_init();
    sound_init();

    thread5_game_loop(NULL);
#ifdef TARGET_WEB
    /*for (int i = 0; i < atoi(argv[1]); i++) {
        game_loop_one_iteration();
    }*/
    inited = 1;
#else
    inited = 1;
    while (1) {
        wm_api->main_loop(produce_one_frame);
    }
#endif
}

#if defined(_WIN32) || defined(_WIN64)
#include <stdbool.h>
int WINAPI WinMain(UNUSED HINSTANCE hInstance, UNUSED HINSTANCE hPrevInstance, UNUSED LPSTR pCmdLine, UNUSED int nCmdShow) {
    AttachConsole(ATTACH_PARENT_PROCESS);

    FILE *fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);

    main_func(NULL);
    
    return 0;
}
#else
int main(int argc, const char *argv[]) {
    main_func(argc > 1 ? argv[1] : NULL);
    return 0;
}
#endif