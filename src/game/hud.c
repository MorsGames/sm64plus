#include <PR/ultratypes.h>

#include "sm64.h"
#include "actors/common1.h"
#include "gfx_dimensions.h"
#include "game_init.h"
#include "level_update.h"
#include "camera.h"
#include "print.h"
#include "ingame_menu.h"
#include "hud.h"
#include "segment2.h"
#include "area.h"
#include "save_file.h"
#include "print.h"

#include "text_strings.h"

#include "settings.h"

/* @file hud.c
 * This file implements HUD rendering and power meter animations.
 * That includes stars, lives, coins, camera status, power meter, timer
 * cannon reticle, and the unused keys.
 **/

struct PowerMeterHUD {
    s8 animation;
    s16 x;
    s16 y;
    f32 unused;
};

struct UnusedHUDStruct {
    u32 unused1;
    u16 unused2;
    u16 unused3;
};

struct CameraHUD {
    s16 status;
};

// Stores health segmented value defined by numHealthWedges
// When the HUD is rendered this value is 8, full health.
static s16 sPowerMeterStoredHealth;

static struct PowerMeterHUD sPowerMeterHUD = {
    POWER_METER_HIDDEN,
    140,
    166,
    1.0,
};

// Power Meter timer that keeps counting when it's visible.
// Gets reset when the health is filled and stops counting
// when the power meter is hidden.
s32 sPowerMeterVisibleTimer = 0;

float sStarGetAlpha = 0.0f;
float sStarGetBounce = 0.0f;
float sStarGetSpeed = 0.0f;

// Custom left and right snapping functions
s32 get_left(s32 value) {
    if (config4by3Hud || configAspectRatio == 1) {
        return value;
    }
    else {
        return GFX_DIMENSIONS_RECT_FROM_LEFT_EDGE(value);
    }
}
s32 get_right(s32 value) {
    if (config4by3Hud || configAspectRatio == 1) {
        return SCREEN_WIDTH-value;
    }
    else {
        return GFX_DIMENSIONS_RECT_FROM_RIGHT_EDGE(value);
    }
}

f32 get_power_meter_x() {
    if (configHudLayout == 2) {
        return get_right(46);
    }
    else if (configHudLayout == 1) {
        return SCREEN_WIDTH/2;
    } else {
        return sPowerMeterHUD.x;
    }
}

UNUSED static struct UnusedHUDStruct sUnusedHUDValues = { 0x00, 0x0A, 0x00 };

static struct CameraHUD sCameraHUD = { CAM_STATUS_NONE };

static u32 sPowerMeterLastRenderTimestamp;
static s16 sPowerMeterLastY;
static Gfx *sPowerMeterDisplayListPos;

static Gfx *sStarGetDisplayListPos;

void patch_interpolated_hud(void) {
    if (sPowerMeterDisplayListPos != NULL) {
        Mtx *mtx = alloc_display_list(sizeof(Mtx));
        guTranslate(mtx, get_power_meter_x(), (f32) sPowerMeterHUD.y, 0);
        gSPMatrix(sPowerMeterDisplayListPos, VIRTUAL_TO_PHYSICAL(mtx),
              G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
        sPowerMeterDisplayListPos = NULL;
    }
}

/**
 * Renders a rgba16 16x16 glyph texture from a table list.
 */
void render_hud_tex_lut(s32 x, s32 y, u8 *texture) {
    gDPPipeSync(gDisplayListHead++);
    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, texture);
    gSPDisplayList(gDisplayListHead++, &dl_hud_img_load_tex_block);
    gSPTextureRectangle(gDisplayListHead++, x << 2, y << 2, (x + 15) << 2, (y + 15) << 2,
                        G_TX_RENDERTILE, 0, 0, 4 << 10, 1 << 10);
}

/**
 * Renders a rgba16 8x8 glyph texture from a table list.
 */
void render_hud_small_tex_lut(s32 x, s32 y, u8 *texture) {
    gDPSetTile(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0,
                G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD);
    gDPTileSync(gDisplayListHead++);
    gDPSetTile(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 2, 0, G_TX_RENDERTILE, 0,
                G_TX_CLAMP, 3, G_TX_NOLOD, G_TX_CLAMP, 3, G_TX_NOLOD);
    gDPSetTileSize(gDisplayListHead++, G_TX_RENDERTILE, 0, 0, (8 - 1) << G_TEXTURE_IMAGE_FRAC, (8 - 1) << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(gDisplayListHead++);
    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, texture);
    gDPLoadSync(gDisplayListHead++);
    gDPLoadBlock(gDisplayListHead++, G_TX_LOADTILE, 0, 0, 8 * 8 - 1, CALC_DXT(8, G_IM_SIZ_16b_BYTES));
    gSPTextureRectangle(gDisplayListHead++, x << 2, y << 2, (x + 7) << 2, (y + 7) << 2, G_TX_RENDERTILE,
                        0, 0, 4 << 10, 1 << 10);
}

/**
 * Renders power meter health segment texture using a table list.
 */
void render_power_meter_health_segment(s16 numHealthWedges) {
    u8 *(*healthLUT)[];

    healthLUT = segmented_to_virtual(&power_meter_health_segments_lut);

    gDPPipeSync(gDisplayListHead++);
    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1,
                       (*healthLUT)[numHealthWedges - 1]);
    gDPLoadSync(gDisplayListHead++);
    gDPLoadBlock(gDisplayListHead++, G_TX_LOADTILE, 0, 0, 32 * 32 - 1, CALC_DXT(32, G_IM_SIZ_16b_BYTES));
    gSP1Triangle(gDisplayListHead++, 0, 1, 2, 0);
    gSP1Triangle(gDisplayListHead++, 0, 2, 3, 0);
}

/**
 * Renders power meter display lists.
 * That includes the "POWER" base and the colored health segment textures.
 */
void render_dl_power_meter(s16 numHealthWedges) {
    Mtx *mtx;
    f32 interpolatedY;

    mtx = alloc_display_list(sizeof(Mtx));

    if (mtx == NULL) {
        return;
    }

    if (gGlobalTimer == sPowerMeterLastRenderTimestamp + 1) {
        interpolatedY = (sPowerMeterLastY + sPowerMeterHUD.y) / 2.0f;
    } else {
        interpolatedY = sPowerMeterHUD.y;
    }

    guTranslate(mtx, get_power_meter_x(), interpolatedY, 0);

    sPowerMeterLastY = sPowerMeterHUD.y;
    sPowerMeterLastRenderTimestamp = gGlobalTimer;
    sPowerMeterDisplayListPos = gDisplayListHead;

    gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(mtx++),
              G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
    gSPDisplayList(gDisplayListHead++, &dl_power_meter_base);

    if (numHealthWedges != 0) {
        gSPDisplayList(gDisplayListHead++, &dl_power_meter_health_segments_begin);
        render_power_meter_health_segment(numHealthWedges);
        gSPDisplayList(gDisplayListHead++, &dl_power_meter_health_segments_end);
    }

    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
}

/**
 * Power meter animation called when there's less than 8 health segments
 * Checks its timer to later change into deemphasizing mode.
 */
void animate_power_meter_emphasized(void) {
    s16 hudDisplayFlags;
    hudDisplayFlags = gHudDisplay.flags;

    if (!(hudDisplayFlags & HUD_DISPLAY_FLAG_EMPHASIZE_POWER)) {
        if (sPowerMeterVisibleTimer == 45.0) {
            sPowerMeterHUD.animation = POWER_METER_DEEMPHASIZING;
        }
    } else {
        sPowerMeterVisibleTimer = 0;
    }
}

/**
 * Power meter animation called after emphasized mode.
 * Moves power meter y pos speed until it's at 200 to be visible.
 */
static void animate_power_meter_deemphasizing(void) {
    s16 speed = 5;

    if (sPowerMeterHUD.y >= 181) {
        speed = 3;
    }

    if (sPowerMeterHUD.y >= 191) {
        speed = 2;
    }

    if (sPowerMeterHUD.y >= 196) {
        speed = 1;
    }

    sPowerMeterHUD.y += speed;

    if (sPowerMeterHUD.y >= 201) {
        sPowerMeterHUD.y = 200;
        sPowerMeterHUD.animation = POWER_METER_VISIBLE;
    }
}

/**
 * Power meter animation called when there's 8 health segments.
 * Moves power meter y pos quickly until it's at 301 to be hidden.
 */
static void animate_power_meter_hiding(void) {
    sPowerMeterHUD.y += 20;
    if (sPowerMeterHUD.y >= 301) {
        sPowerMeterHUD.animation = POWER_METER_HIDDEN;
        sPowerMeterVisibleTimer = 0;
    }
}

/**
 * Handles power meter actions depending of the health segments values.
 */
void handle_power_meter_actions(s16 numHealthWedges) {
    // Show power meter if health is not full, less than 8
    if (numHealthWedges < 8 && sPowerMeterStoredHealth == 8 && sPowerMeterHUD.animation == POWER_METER_HIDDEN) {
        sPowerMeterHUD.animation = POWER_METER_EMPHASIZED;
        sPowerMeterHUD.y = 166;
    }

    // Show power meter if health is full, has 8
    if (numHealthWedges == 8 && sPowerMeterStoredHealth == 7) {
        sPowerMeterVisibleTimer = 0;
    }

    // After health is full, hide power meter
    if (numHealthWedges == 8 && sPowerMeterVisibleTimer > 45.0) {
        sPowerMeterHUD.animation = POWER_METER_HIDING;
    }

    // Update to match health value
    sPowerMeterStoredHealth = numHealthWedges;

    // If Mario is swimming, keep power meter visible
    if (gPlayerCameraState->action & ACT_FLAG_SWIMMING) {
        if (sPowerMeterHUD.animation == POWER_METER_HIDDEN
            || sPowerMeterHUD.animation == POWER_METER_EMPHASIZED) {
            sPowerMeterHUD.animation = POWER_METER_DEEMPHASIZING;
            sPowerMeterHUD.y = 166;
        }
        sPowerMeterVisibleTimer = 0;
    }
}

/**
 * Renders the power meter that shows when Mario is in underwater
 * or has taken damage and has less than 8 health segments.
 * And calls a power meter animation function depending of the value defined.
 */
void render_hud_power_meter(void) {
    s16 shownHealthWedges = gHudDisplay.wedges;

    if ((configHudLayout < 2) && (!configAlwaysShowHealth)) {
        if (sPowerMeterHUD.animation != POWER_METER_HIDING) {
            handle_power_meter_actions(shownHealthWedges);
        }

        if (sPowerMeterHUD.animation == POWER_METER_HIDDEN) {
            return;
        }

        switch (sPowerMeterHUD.animation) {
            case POWER_METER_EMPHASIZED:
                animate_power_meter_emphasized();
                break;
            case POWER_METER_DEEMPHASIZING:
                animate_power_meter_deemphasizing();
                break;
            case POWER_METER_HIDING:
                animate_power_meter_hiding();
                break;
            default:
                break;
        }
    }
    else {
        sPowerMeterHUD.y = 200;
    }

    render_dl_power_meter(shownHealthWedges);

    if ((configHudLayout < 2) && (!configAlwaysShowHealth)) {
        sPowerMeterVisibleTimer += 1;
    }
}

#ifdef VERSION_JP
#define HUD_TOP_Y 210
#else
#define HUD_TOP_Y 209
#endif

#define HUD_TOP_Y_NEW 212
#define HUD_LEFT_X 22

/**
 * Renders the amount of lives Mario has.
 */
void render_hud_mario_lives(void) {
    s32 x;
    s32 y;
    if (configHudLayout == 2) {
        x = get_left(HUD_LEFT_X)+64;
        y = HUD_TOP_Y_NEW;
    }
    else if (configHudLayout == 1) {
        x = get_left(HUD_LEFT_X);
        y = HUD_TOP_Y_NEW;
    }
    else {
        x = get_left(22);
        y = HUD_TOP_Y;
    }
    print_text(x, y, ","); // 'Mario Head' glyph
    if (configHudLayout > 0) {
        print_text(x+17, y, "*"); // 'X' glyph
        print_text_fmt_int(x+33, y, configAddZeroes ? "%02d" : "%d", gHudDisplay.lives);
    }
    else {
        print_text(x+16, y, "*"); // 'X' glyph
        print_text_fmt_int(x+32, y, "%d", gHudDisplay.lives);
    }
}

#define HUD_COIN_X 168

/**
 * Renders the amount of coins collected.
 */
void render_hud_coins(void) {
    if (configHudLayout > 0) {
        print_text(get_left(HUD_LEFT_X), HUD_TOP_Y_NEW-18, "+"); // 'Coin' glyph
        if (configAddZeroes) {
            print_text_fmt_int(get_left(HUD_LEFT_X)+17, HUD_TOP_Y_NEW-18, "%03d", gHudDisplay.coins);
        }
        else {
            print_text(get_left(HUD_LEFT_X)+17, HUD_TOP_Y_NEW-18, "*"); // 'X' glyph
            print_text_fmt_int(get_left(HUD_LEFT_X)+33, HUD_TOP_Y_NEW-18, "%d", gHudDisplay.coins);
        }
    }
    else {
        print_text(HUD_COIN_X, HUD_TOP_Y, "+"); // 'Coin' glyph
        if (configAddZeroes) {
            print_text_fmt_int(HUD_COIN_X+16, HUD_TOP_Y, "%03d", gHudDisplay.coins);
        }
        else {
            print_text(HUD_COIN_X+16, HUD_TOP_Y, "*"); // 'X' glyph
            print_text_fmt_int(HUD_COIN_X+30, HUD_TOP_Y, "%d", gHudDisplay.coins);
        }
    }
}

#ifdef VERSION_JP
#define HUD_STARS_X 73
#else
#define HUD_STARS_X 78
#endif

#define HUD_STARS_X_NEW 76

/**
 * Renders the amount of stars collected.
 * Disables "X" glyph when Mario has 100 stars or more.
 */
void render_hud_stars(void) {
    s8 showX = 0;

    if (gHudFlash == 1 && gGlobalTimer & 0x08) {
        return;
    }
    
    if (configHudLayout == 2) {
        if (!configAddZeroes && gHudDisplay.stars < 10) {
            print_text(get_left(HUD_LEFT_X), HUD_TOP_Y_NEW, "-"); // 'Star' glyph
            print_text(get_left(HUD_LEFT_X) + 17, HUD_TOP_Y_NEW, "*"); // 'X' glyph
            print_text_fmt_int(get_left(HUD_LEFT_X) + 33, HUD_TOP_Y_NEW, "%d", gHudDisplay.stars);
        }
        else if (!configAddZeroes && gHudDisplay.stars < 100) {
            print_text(get_left(HUD_LEFT_X), HUD_TOP_Y_NEW, "-"); // 'Star' glyph
            print_text(get_left(HUD_LEFT_X) + 17, HUD_TOP_Y_NEW, "*"); // 'X' glyph
            print_text_fmt_int(get_left(HUD_LEFT_X) + 33, HUD_TOP_Y_NEW, "%d", gHudDisplay.stars);
        }
        else {
            print_text(get_left(HUD_LEFT_X), HUD_TOP_Y_NEW, "-"); // 'Star' glyph
            print_text_fmt_int(get_left(HUD_LEFT_X) + 17, HUD_TOP_Y_NEW, configAddZeroes ? "%03d" : "%d", gHudDisplay.stars);
        }
    }
    else if (configHudLayout == 1) {
        if (!configAddZeroes && gHudDisplay.stars < 10) {
            print_text(get_right(HUD_STARS_X_NEW) + 6, HUD_TOP_Y_NEW, "-"); // 'Star' glyph
            print_text(get_right(HUD_STARS_X_NEW) + 23, HUD_TOP_Y_NEW, "*"); // 'X' glyph
            print_text_fmt_int(get_right(HUD_STARS_X_NEW) + 39, HUD_TOP_Y_NEW, "%d", gHudDisplay.stars);
        }
        else if (!configAddZeroes && gHudDisplay.stars < 100) {
            print_text(get_right(HUD_STARS_X_NEW) - 4, HUD_TOP_Y_NEW, "-"); // 'Star' glyph
            print_text(get_right(HUD_STARS_X_NEW) + 11, HUD_TOP_Y_NEW, "*"); // 'X' glyph
            print_text_fmt_int(get_right(HUD_STARS_X_NEW) + 27, HUD_TOP_Y_NEW, "%d", gHudDisplay.stars);
        }
        else {
            print_text(get_right(HUD_STARS_X_NEW), HUD_TOP_Y_NEW, "-"); // 'Star' glyph
            print_text_fmt_int(get_right(HUD_STARS_X_NEW) + 15, HUD_TOP_Y_NEW, configAddZeroes ? "%03d" : "%d", gHudDisplay.stars);
        }
    }
    else {
        if (!configAddZeroes && gHudDisplay.stars < 100) {
            showX = 1;
        }

        print_text(get_right(HUD_STARS_X), HUD_TOP_Y, "-"); // 'Star' glyph
        if (showX == 1) {
            print_text(get_right(HUD_STARS_X) + 16, HUD_TOP_Y, "*"); // 'X' glyph
        }
        print_text_fmt_int((showX * 14) + get_right(HUD_STARS_X - 16),
                        HUD_TOP_Y, configAddZeroes ? "%03d" : "%d", gHudDisplay.stars);
    }
}

/**
 * Unused function that renders the amount of keys collected.
 * Leftover function from the beta version of the game.
 */
void render_hud_keys(void) {
    s16 i;

    for (i = 0; i < gHudDisplay.keys; i++) {
        print_text((i * 16) + 220, 142, "/"); // unused glyph - beta key
    }
}

/**
 * Renders the timer when Mario start sliding in PSS.
 */
void render_hud_timer(void) {
    u8 *(*hudLUT)[58];
    u16 timerValFrames;
    u16 timerMins;
    u16 timerSecs;
    u16 timerFracSecs;

    hudLUT = segmented_to_virtual(&main_hud_lut);
    timerValFrames = gHudDisplay.timer;
#ifdef VERSION_EU
    switch (eu_get_language()) {
        case LANGUAGE_ENGLISH:
            print_text(get_right(150), 185, "TIME");
            break;
        case LANGUAGE_FRENCH:
            print_text(get_right(155), 185, "TEMPS");
            break;
        case LANGUAGE_GERMAN:
            print_text(get_right(150), 185, "ZEIT");
            break;
    }
#endif
    timerMins = timerValFrames / (30 * 60);
    timerSecs = (timerValFrames - (timerMins * 1800)) / 30;

    timerFracSecs = ((timerValFrames - (timerMins * 1800) - (timerSecs * 30)) & 0xFFFF) / 3;

        if (configHudLayout != 0) {
            s32 add;
            if (configHudLayout == 2)
                add = 157;
            else
                add = 0;
            print_text(get_right(151), HUD_TOP_Y_NEW-18-add, "TIME");
            print_text_fmt_int(get_right(92), HUD_TOP_Y_NEW-18-add, "%0d", timerMins);
            print_text_fmt_int(get_right(72), HUD_TOP_Y_NEW-18-add, "%02d", timerSecs);
            print_text_fmt_int(get_right(38), HUD_TOP_Y_NEW-18-add, "%d", timerFracSecs);
            gSPDisplayList(gDisplayListHead++, dl_hud_img_begin);
            render_hud_tex_lut(get_right(82), 23+add, (*hudLUT)[GLYPH_APOSTROPHE]);
            render_hud_tex_lut(get_right(47), 23+add, (*hudLUT)[GLYPH_DOUBLE_QUOTE]);
        }
        else {
#ifndef VERSION_EU
            print_text(get_right(150), 185, "TIME");
#endif
            print_text_fmt_int(get_right(91), 185, "%0d", timerMins);
            print_text_fmt_int(get_right(71), 185, "%02d", timerSecs);
            print_text_fmt_int(get_right(37), 185, "%d", timerFracSecs);
            gSPDisplayList(gDisplayListHead++, dl_hud_img_begin);
            render_hud_tex_lut(get_right(81), 32, (*hudLUT)[GLYPH_APOSTROPHE]);
            render_hud_tex_lut(get_right(46), 32, (*hudLUT)[GLYPH_DOUBLE_QUOTE]);
        }
    gSPDisplayList(gDisplayListHead++, dl_hud_img_end);
}

/**
 * Sets HUD status camera value depending of the actions
 * defined in update_camera_status.
 */
void set_hud_camera_status(s16 status) {
    sCameraHUD.status = status;
}

/**
 * Renders camera HUD glyphs using a table list, depending of
 * the camera status called, a defined glyph is rendered.
 */
void render_hud_camera_status(void) {
    u8 *(*cameraLUT)[6];
    s32 x;
    s32 y;

    cameraLUT = segmented_to_virtual(&main_hud_camera_lut);
    if (configHudLayout == 1) {
        x = get_right(53);
        y = 208;
    }
    else {
        x = get_right(54);
        y = 205;
    }

    if (sCameraHUD.status == CAM_STATUS_NONE) {
        return;
    }

    gSPDisplayList(gDisplayListHead++, dl_hud_img_begin);
    render_hud_tex_lut(x, y, (*cameraLUT)[GLYPH_CAM_CAMERA]);

    switch (sCameraHUD.status & CAM_STATUS_MODE_GROUP) {
        case CAM_STATUS_MARIO:
            render_hud_tex_lut(x + 16, y, (*cameraLUT)[GLYPH_CAM_MARIO_HEAD]);
            break;
        case CAM_STATUS_LAKITU:
            render_hud_tex_lut(x + 16, y, (*cameraLUT)[GLYPH_CAM_LAKITU_HEAD]);
            break;
        case CAM_STATUS_FIXED:
            render_hud_tex_lut(x + 16, y, (*cameraLUT)[GLYPH_CAM_FIXED]);
            break;
    }

    switch (sCameraHUD.status & CAM_STATUS_C_MODE_GROUP) {
        case CAM_STATUS_C_DOWN:
            render_hud_small_tex_lut(x + 4, y + 16, (*cameraLUT)[GLYPH_CAM_ARROW_DOWN]);
            break;
        case CAM_STATUS_C_UP:
            render_hud_small_tex_lut(x + 4, y - 8, (*cameraLUT)[GLYPH_CAM_ARROW_UP]);
            break;
    }

    gSPDisplayList(gDisplayListHead++, dl_hud_img_end);
}

void render_hud_level_stars(s16 starCount) {
    s32 i;

    u8 flag = 1;
    u8 starFlags = save_file_get_star_flags(gCurrSaveFileNum - 1, gCurrCourseNum - 1);

    u8 starText[] = { GLYPH_STAR, GLYPH_SPACE };

    for (i = 0; i < starCount; i++, flag <<= 1) {
        gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
        if (starFlags & flag) {
            gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255 * (1-sStarGetAlpha));
        }
        else {
            gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 63 * (1-sStarGetAlpha));
        }
        print_hud_lut_string(HUD_LUT_GLOBAL, get_left(HUD_LEFT_X) + i*16, SCREEN_HEIGHT-35, starText);
        gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
        gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
    }
}

void hide_you_got_a_star() {
    gHudDisplay.starGet = 0;
    sStarGetSpeed = 0.0f;
    sStarGetAlpha = 0.0f;
    sStarGetBounce = 0.0f;
}

void render_you_got_a_star(u32 secondFrame) {

    if (!gHudDisplay.starGet)
        return;
    
    u8 youGotAStar[] = { TEXT_YOU_GOT_A_STAR };

    // Set the bounce speed at the start
    if (sStarGetAlpha == 0.0f)
        sStarGetSpeed = -4.0f;

    // Gravity
    sStarGetSpeed += 0.25f;

    // Add the speed to the position and limit it
    sStarGetBounce = MIN(sStarGetBounce + sStarGetSpeed, 0.0f);

    Mtx *matrix = (Mtx *) alloc_display_list(sizeof(Mtx));
    if (matrix) {
        if (stay_in_level() || restart_level_after_star()) {
            create_dl_translation_matrix(MENU_MTX_PUSH, (get_left(0)*2) / 2, 88.0f - (1.0f - sStarGetAlpha) * 32.0f, 0);
            guScale(matrix, (SCREEN_WIDTH + get_right(0)*2) / 130.0f, 64.0f * sStarGetAlpha / 80.0f, 1.f);
        }
        else {
            create_dl_translation_matrix(MENU_MTX_PUSH, (get_left(0)*2) / 2, 88.0f - (1.0f - sStarGetAlpha) * 16.0f, 0);
            guScale(matrix, (SCREEN_WIDTH + get_right(0)*2) / 130.0f, 32.0f * sStarGetAlpha / 80.0f, 1.f);
        }
    }

    // Rendering
    if (secondFrame == 1) {
        if (sStarGetDisplayListPos != NULL) {
            gDPSetEnvColor(sStarGetDisplayListPos++, 255, 255, 255, 255 * sStarGetAlpha);
            if (stay_in_level() || restart_level_after_star())
                print_hud_lut_string_to_displaylist(HUD_LUT_GLOBAL, SCREEN_WIDTH / 2 - 78, 159 + sStarGetBounce, youGotAStar, sStarGetDisplayListPos);
            else
                print_hud_lut_string_to_displaylist(HUD_LUT_GLOBAL, SCREEN_WIDTH / 2 - 78, 160 + sStarGetBounce, youGotAStar, sStarGetDisplayListPos);
            sStarGetDisplayListPos = NULL;
        }
    }
    if (secondFrame == 0) {

        // Black box
        if (matrix) {
            gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(matrix), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
            gDPSetEnvColor(gDisplayListHead++, 0, 0, 0, 255 * sStarGetAlpha / 2);
            gSPDisplayList(gDisplayListHead++, dl_draw_text_bg_box);
            gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
        }

        // You got a star!

        gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
        sStarGetDisplayListPos = gDisplayListHead;
        gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255 * sStarGetAlpha);
        if (stay_in_level() || restart_level_after_star()) 
            print_hud_lut_string(HUD_LUT_GLOBAL, SCREEN_WIDTH / 2 - 78, 159 + sStarGetBounce, youGotAStar);
        else
            print_hud_lut_string(HUD_LUT_GLOBAL, SCREEN_WIDTH / 2 - 78, 160 + sStarGetBounce, youGotAStar);
        gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
        gSPDisplayList(gDisplayListHead++, dl_ia_text_end);

        // Course and act name
        if (stay_in_level() || restart_level_after_star()) {
            void **courseNameTbl;
            u8 *courseName;
            void **actNameTbl;
            u8 courseIndex;

            // Gonna keep these ifdefs in case I decide to add compatibility with other region ROMs.
#ifndef VERSION_EU
            courseNameTbl = segmented_to_virtual(seg2_course_name_table);
            actNameTbl = segmented_to_virtual(seg2_act_name_table);
#endif

            courseIndex = gCurrCourseNum - 1;

#ifdef VERSION_EU
            switch (gInGameLanguage) {
                case LANGUAGE_ENGLISH:
                    actNameTbl = segmented_to_virtual(act_name_table_eu_en);
                    courseNameTbl = segmented_to_virtual(course_name_table_eu_en);
                    break;
                case LANGUAGE_FRENCH:
                    actNameTbl = segmented_to_virtual(act_name_table_eu_fr);
                    courseNameTbl = segmented_to_virtual(course_name_table_eu_fr);
                    break;
                case LANGUAGE_GERMAN:
                    actNameTbl = segmented_to_virtual(act_name_table_eu_de);
                    courseNameTbl = segmented_to_virtual(course_name_table_eu_de);
                    break;
            }
#endif

            gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);

            gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255 * sStarGetAlpha);

            courseName = segmented_to_virtual(courseNameTbl[courseIndex]);

            if (courseIndex < COURSE_STAGES_COUNT && gCollectedStar < 6) {
                u8 *actName = segmented_to_virtual(actNameTbl[(gCurrCourseNum - 1) * 6 + gCollectedStar]);
                print_generic_string(get_str_x_pos_from_center(SCREEN_WIDTH/2, actName, 1.0f), 28, actName);
            }
            else {
                u8 actName[] = { TEXT_POWER_STAR_PLACEHOLDER };
                print_generic_string(get_str_x_pos_from_center(SCREEN_WIDTH/2, actName, 1.0f), 28, actName);
            }
            print_generic_string(get_str_x_pos_from_center(SCREEN_WIDTH/2, &courseName[3], 1.0f), 44, &courseName[3]);

            gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
        }
    }

    // Using the speed as a timer since we never actually stop the speed from increasing
    if (sStarGetSpeed > 31.0f) {

        // Reduce the alpha, no one likes suddenly disappearing UI elements
        sStarGetAlpha -= 0.0625f;

        // If finally fully transparent, reset stuff
        if (sStarGetAlpha <= 0.0f) {
            hide_you_got_a_star();
        }
    }
    // Fade in
    else if (sStarGetAlpha < 1.0f)
        sStarGetAlpha += 0.0625f;
}

/**
 * Render HUD strings using hudDisplayFlags with it's render functions,
 * excluding the cannon reticle which detects a camera preset for it.
 */
void render_hud(void) {
    s16 hudDisplayFlags;
#ifdef VERSION_EU
    Mtx *mtx;
#endif

    hudDisplayFlags = gHudDisplay.flags;

    if (hudDisplayFlags == HUD_DISPLAY_NONE) {
        sPowerMeterHUD.animation = POWER_METER_HIDDEN;
        sPowerMeterStoredHealth = 8;
        sPowerMeterVisibleTimer = 0;
    } else {
#ifdef VERSION_EU
        // basically create_dl_ortho_matrix but guOrtho screen width is different

        mtx = alloc_display_list(sizeof(*mtx));
        if (mtx == NULL) {
            return;
        }
        create_dl_identity_matrix();
        guOrtho(mtx, -16.0f, SCREEN_WIDTH + 16, 0, SCREEN_HEIGHT, -10.0f, 10.0f, 1.0f);
        gSPPerspNormalize(gDisplayListHead++, 0xFFFF);
        gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(mtx),
                G_MTX_PROJECTION | G_MTX_MUL | G_MTX_NOPUSH);
#else
        create_dl_ortho_matrix();
#endif

        if (gCurrentArea != NULL && gCurrentArea->camera->mode == CAMERA_MODE_INSIDE_CANNON) {
            render_hud_cannon_reticle();
        }

        if (hudDisplayFlags & HUD_DISPLAY_FLAG_LIVES && !(save_file_get_flags() & SAVE_FLAG_HARDCORE_MODE) && configLifeMode != 1) {
            render_hud_mario_lives();
        }

        if (hudDisplayFlags & HUD_DISPLAY_FLAG_COIN_COUNT) {
            render_hud_coins();
        }

        if (hudDisplayFlags & HUD_DISPLAY_FLAG_STAR_COUNT) {
            render_hud_stars();
        }

        if (hudDisplayFlags & HUD_DISPLAY_FLAG_KEYS) {
            render_hud_keys();
        }

        if (hudDisplayFlags & HUD_DISPLAY_FLAG_CAMERA_AND_POWER) {
            if (!(save_file_get_flags() & SAVE_FLAG_DAREDEVIL_MODE))
                render_hud_power_meter();
            render_hud_camera_status();
        }

        if (hudDisplayFlags & HUD_DISPLAY_FLAG_TIMER) {
            render_hud_timer();
        }

        if (configHudStars && gCurrDemoInput == NULL
            && gCurrLevelNum != LEVEL_CASTLE_GROUNDS && gCurrLevelNum != LEVEL_CASTLE && gCurrLevelNum != LEVEL_CASTLE_COURTYARD
            && gCurrLevelNum != LEVEL_BOWSER_1 && gCurrLevelNum != LEVEL_BOWSER_2 && gCurrLevelNum != LEVEL_BOWSER_3 && gCurrLevelNum != LEVEL_ENDING)
        {
            if (gCurrLevelNum == LEVEL_BITDW || gCurrLevelNum == LEVEL_BITFS || gCurrLevelNum == LEVEL_BITS ||
            gCurrLevelNum == LEVEL_COTMC || gCurrLevelNum == LEVEL_TOTWC || gCurrLevelNum == LEVEL_VCUTM ||
            gCurrLevelNum == LEVEL_WMOTR || gCurrLevelNum == LEVEL_SA) {
                render_hud_level_stars(1);
            }
            else if (gCurrLevelNum == LEVEL_PSS) {
                render_hud_level_stars(2);
            }
            else {
                render_hud_level_stars(6 + (configShow100CoinStar || (save_file_get_star_flags(gCurrSaveFileNum - 1, gCurrCourseNum - 1) & (1 << 6))));
            }
        }

        if (gMarioWillDie) {
            print_text_centered(SCREEN_WIDTH / 2, 160, "NOW YOU WILL DIE");
        }

        if (configStayInCourse && gCurrLevelNum != LEVEL_BOWSER_1 && gCurrLevelNum != LEVEL_BOWSER_2 && gCurrLevelNum != LEVEL_BOWSER_3) {
            render_you_got_a_star(0);
        }
    }
}