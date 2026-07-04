/**
 * @file lvgl_scenario5.c — 场景五：G3 立体线框立方体（SBS 左右眼）
 *
 * Recreates 001_Stereo_Cube_for_G3_1m: 2560x720 SBS @ 60fps, black bg,
 * neon-green wireframe cube @ ~1 m, 360 deg / 10 s Y rotation.
 */

#include "lvgl/lvgl.h"
#include "lvgl_demos.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS && LV_USE_DRAW_GPU_RENDERER

#include "draw/gpu_renderer/lv_draw_gpu_renderer.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef LVGL_S5_CUBE_SIZE
    #define LVGL_S5_CUBE_SIZE 420.0f
#endif

#ifndef LVGL_S5_VIEW_DIST
    #define LVGL_S5_VIEW_DIST 1000.0f
#endif

#ifndef LVGL_S5_EYE_SEP
    #define LVGL_S5_EYE_SEP 32.0f
#endif

#ifndef LVGL_S5_ROT_DEG_PER_SEC
    #define LVGL_S5_ROT_DEG_PER_SEC 36.0f
#endif

#ifndef LVGL_S5_STEREO
    #define LVGL_S5_STEREO 0
#endif

#ifndef LVGL_S5_HUD
    #define LVGL_S5_HUD 0
#endif

static lv_obj_t * g_cube;
static lv_obj_t * g_vp_l;
static lv_obj_t * g_vp_r;
static lv_obj_t * g_scene;
static uint32_t s_last_ms;
static float g_yaw_deg;
static bool g_stereo;

static bool s5_hud_enabled(void)
{
#if LVGL_S5_HUD
    return true;
#else
    const char * env = getenv("LVGL_S5_HUD");
    return env && (env[0] == '1' || env[0] == 'y' || env[0] == 'Y');
#endif
}

static bool s5_turbo_enabled(void)
{
    const char * env = getenv("LVGL_DRM_TURBO");
    return env && (env[0] == '1' || env[0] == 'y' || env[0] == 'Y');
}

static bool s5_stereo_enabled(void)
{
#if LVGL_S5_STEREO
    return true;
#else
    const char * env = getenv("LVGL_S5_STEREO");
    return env && (env[0] == '1' || env[0] == 'y' || env[0] == 'Y');
#endif
}

static void s5_setup_camera(lv_obj_t * cam, float eye_x)
{
    /* Match scenario 3/4: camera on +Z looking toward -Z scene. */
    lv_3dcamera_set_perspective(cam, 36.0f, 10.0f, 5000.0f);
    lv_3dcamera_look_at(cam,
                        (lv_vec3_t) { eye_x, 0.0f, 520.0f },
                        (lv_vec3_t) { 0.0f, 0.0f, -LVGL_S5_VIEW_DIST },
                        (lv_vec3_t) { 0.0f, 1.0f, 0.0f });
}

static void s5_setup_viewport(lv_obj_t * vp, lv_obj_t * cam, lv_align_t align)
{
    lv_obj_set_size(vp, lv_pct(g_stereo ? 50 : 100), lv_pct(100));
    lv_obj_align(vp, align, 0, 0);
    lv_obj_set_style_bg_opa(vp, LV_OPA_0, 0);
    lv_obj_set_style_border_width(vp, 0, 0);
    lv_3dviewport_set_camera(vp, cam);
    lv_3dviewport_set_scene(vp, g_scene);
}

static void anim_timer_cb(lv_timer_t * t)
{
    LV_UNUSED(t);

    uint32_t now = lv_tick_get();
    float dt = 0.016f;
    if(s_last_ms != 0) {
        dt = (float)(now - s_last_ms) / 1000.0f;
        if(dt < 0.001f) return;
        if(dt > 0.25f) dt = 0.25f;
    }
    s_last_ms = now;

    g_yaw_deg += LVGL_S5_ROT_DEG_PER_SEC * dt;
    if(g_yaw_deg >= 360.0f) g_yaw_deg -= 360.0f;

    lv_3dmesh_set_rotation_y(g_cube, g_yaw_deg);
    lvgl_demos_request_gpu_frame();
}

void lvgl_scenario5_stereo_cube_create(void)
{
    g_stereo = s5_stereo_enabled();

    /* Black backdrop + no 2D overlay (matches G3 reference, max FPS). */
    lv_gpu_renderer_set_ui_mode(LV_GPU_RENDERER_UI_WIREFRAME_BENCH);
    lv_gpu_renderer_set_skip_alpha_probe(true);
    lv_gpu_renderer_set_overlay_2d_enable(false);
    if(!g_stereo) {
        setenv("LVGL_DRM_TURBO", "1", 0);
    }

    lv_obj_t * scr = lv_screen_active();
    /* OPA_0 — keep SW buffer transparent so overlay stays skipped. */
    lv_obj_set_style_bg_opa(scr, LV_OPA_0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    g_scene = lv_3dscene_create(scr);
    lv_obj_set_style_bg_opa(g_scene, LV_OPA_0, 0);

    g_cube = lv_3dmesh_create(g_scene);
    lv_3dmesh_set_box(g_cube, LVGL_S5_CUBE_SIZE, LVGL_S5_CUBE_SIZE, LVGL_S5_CUBE_SIZE);
    lv_3dmesh_set_wireframe(g_cube, true);
    lv_3dmesh_set_color(g_cube, lv_color_hex(0x00FF00));
    lv_3dmesh_set_position(g_cube, 0.0f, 0.0f, -LVGL_S5_VIEW_DIST);

    lv_obj_t * cam_l = lv_3dcamera_create(scr);
    s5_setup_camera(cam_l, g_stereo ? -LVGL_S5_EYE_SEP : 0.0f);

    g_vp_l = lv_3dviewport_create(scr);
    s5_setup_viewport(g_vp_l, cam_l, g_stereo ? LV_ALIGN_LEFT_MID : LV_ALIGN_CENTER);

    if(g_stereo) {
        lv_obj_t * cam_r = lv_3dcamera_create(scr);
        s5_setup_camera(cam_r, LVGL_S5_EYE_SEP);

        g_vp_r = lv_3dviewport_create(scr);
        s5_setup_viewport(g_vp_r, cam_r, LV_ALIGN_RIGHT_MID);

        lv_obj_t * split = lv_obj_create(scr);
        lv_obj_set_size(split, 2, lv_pct(100));
        lv_obj_align(split, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(split, lv_color_hex(0x303030), 0);
        lv_obj_set_style_bg_opa(split, LV_OPA_60, 0);
        lv_obj_set_style_border_width(split, 0, 0);
        lv_obj_remove_flag(split, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }
    else {
        g_vp_r = NULL;
    }

    if(s5_hud_enabled()) {
        lv_obj_t * tag = lv_label_create(scr);
        lv_label_set_text(tag, g_stereo ? "S5  SBS Stereo  |  G3 1 m" : "S5  Mono Cube  |  G3 1 m  |  LVGL_S5_STEREO=1 for SBS");
        lv_obj_set_style_text_color(tag, lv_color_hex(0x00FF00), 0);
        lv_obj_set_style_bg_color(tag, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(tag, LV_OPA_70, 0);
        lv_obj_set_style_pad_hor(tag, 10, 0);
        lv_obj_set_style_pad_ver(tag, 4, 0);
        lv_obj_set_style_radius(tag, 4, 0);
        lv_obj_align(tag, LV_ALIGN_TOP_MID, 0, 8);
        lv_obj_move_foreground(tag);
        lv_gpu_renderer_set_overlay_2d_enable(true);
    }

    g_yaw_deg = 0.0f;
    s_last_ms = 0;
    lvgl_demos_set_direct_gpu_present(!g_stereo);
    lv_timer_create(anim_timer_cb, 16, NULL);

    lv_obj_invalidate(g_vp_l);
    if(g_vp_r) lv_obj_invalidate(g_vp_r);

    LV_LOG_USER("Scenario5 cube: size=%.0f dist=%.0f sep=%.0f stereo=%d turbo=%d hud=%d",
                LVGL_S5_CUBE_SIZE, LVGL_S5_VIEW_DIST, LVGL_S5_EYE_SEP, g_stereo ? 1 : 0,
                s5_turbo_enabled() ? 1 : 0, s5_hud_enabled() ? 1 : 0);
}

float lvgl_scenario5_get_yaw_deg(void)
{
    return g_yaw_deg;
}

#else

void lvgl_scenario5_stereo_cube_create(void)
{
    printf("LVGL_SCENARIO5: requires LV_USE_3D + LV_USE_DRAW_GPU_RENDERER\n");
}

float lvgl_scenario5_get_yaw_deg(void)
{
    return 0.0f;
}

#endif
