/**
 * @file lvgl_scenario2.c — 场景二：导航车道 + 两侧楼群 parallax 后移 (Phase 2)
 */

#include "lvgl/lvgl.h"
#include "lvgl_demos.h"

#if LV_USE_DRAW_GPU_RENDERER
#include "draw/gpu_renderer/lv_draw_gpu_renderer.h"
#endif

#if LV_USE_3D && LV_USE_3D_WIDGETS && LV_USE_3D_SEGMENT_POOL

#include <stdlib.h>

/** Buildings per side = (3..6) * this factor (default 1). */
#ifndef LVGL_S2_BUILDING_MUL
    #define LVGL_S2_BUILDING_MUL 1
#endif
#define S2_SEGMENT_LENGTH 420.0f
#define S2_CAR_TEX_W      128
#define S2_CAR_TEX_H      266
/* 3D plane size — was 128×266 when tex was 280px wide; scale with tex width. */
#define S2_CAR_PLANE_W     58.0f
#define S2_CAR_PLANE_H    122.0f

static lv_3d_segment_pool_t * g_pool;

static void spawn_building(lv_obj_t * root, float x, float y, float z, float w, float h, float d)
{
    lv_obj_t * m = lv_3dmesh_create(root);
    lv_3dmesh_set_box(m, w, h, d);
    lv_3d_material_t mat;
    lv_3d_material_init(&mat, LV_3D_MAT_SHADED_BOX, lv_color_hex(0x9E9E9E), LV_OPA_COVER);
    mat.top_color = lv_color_hex(0xBDBDBD);
    lv_3dmesh_set_material(m, &mat);
    lv_3dmesh_set_position(m, x, y, z);
}

static lv_obj_t * spawn_solid_box(lv_obj_t * root, float x, float y, float z,
                                  float w, float h, float d, lv_color_t c)
{
    lv_obj_t * m = lv_3dmesh_create(root);
    lv_3dmesh_set_box(m, w, h, d);
    lv_3dmesh_set_wireframe(m, false);
    lv_3dmesh_set_color(m, c);
    lv_3dmesh_set_position(m, x, y, z);
    return m;
}

static void spawn_road_segment(lv_obj_t * root, float seg_base_z)
{
    const float len = S2_SEGMENT_LENGTH * 0.94f;
    const float zc = seg_base_z - len * 0.5f;
    const float road_w = 132.0f;
    const lv_color_t asphalt = lv_color_hex(0x37474F);
    const lv_color_t edge = lv_color_hex(0xECEFF1);
    const lv_color_t dash = lv_color_hex(0xFFEB3B);

    spawn_solid_box(root, 0.0f, 1.5f, zc, road_w, 3.0f, len, asphalt);

    const float edge_x = road_w * 0.5f - 2.0f;
    spawn_solid_box(root, -edge_x, 2.5f, zc, 3.0f, 2.0f, len, edge);
    spawn_solid_box(root, edge_x, 2.5f, zc, 3.0f, 2.0f, len, edge);

    for(int i = 0; i < 7; i++) {
        const float dz = seg_base_z - 30.0f - (float)i * (len / 7.0f);
        spawn_solid_box(root, 0.0f, 3.0f, dz, 10.0f, 2.0f, 24.0f, dash);
    }
}

#if LV_USE_SNAPSHOT
static void spawn_nav_car_plane(lv_obj_t * scene, lv_obj_t * bake_root)
{
    LV_IMAGE_DECLARE(s2_nav_car_img);

    lv_obj_t * src = lv_obj_create(bake_root);
    lv_obj_set_size(src, S2_CAR_TEX_W, S2_CAR_TEX_H);
    lv_obj_remove_flag(src, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(src, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(src, LV_OPA_0, 0);
    lv_obj_set_style_border_width(src, 0, 0);
    lv_obj_set_style_pad_all(src, 0, 0);

    lv_obj_t * img = lv_image_create(src);
    lv_image_set_src(img, &s2_nav_car_img);
    lv_obj_set_size(img, S2_CAR_TEX_W, S2_CAR_TEX_H);
    /* Hood at texture top; nudge down so the front bumper stays inside the viewport. */
    lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 8);

    lv_3d_snapshot_id_t snap = lv_3d_plane_bake(src, LV_3D_PLANE_SRC_SNAPSHOT);

    lv_obj_t * car = lv_3dmesh_create(scene);
    lv_3dmesh_set_box(car, S2_CAR_PLANE_W, S2_CAR_PLANE_H, 2.0f);
    if(snap != LV_3D_SNAPSHOT_ID_NONE) {
        lv_3dmesh_set_plane_snapshot(car, snap);
    }
    else {
        lv_3dmesh_set_wireframe(car, false);
        lv_3dmesh_set_color(car, lv_color_hex(0x1E3A5F));
    }
    lv_3dmesh_set_position(car, 0.0f, 42.0f, 810.0f);
    lv_obj_move_foreground(car);
}
#else
static void spawn_nav_car_plane(lv_obj_t * scene, lv_obj_t * bake_root)
{
    LV_UNUSED(bake_root);
    lv_obj_t * car = lv_3dmesh_create(scene);
    lv_3dmesh_set_box(car, 52.0f, 16.0f, 88.0f);
    lv_3dmesh_set_wireframe(car, true);
    lv_3dmesh_set_color(car, lv_color_hex(0xFFFFFF));
    lv_3dmesh_set_position(car, 0.0f, 28.0f, 720.0f);
}
#endif

static void s2_build_segment(lv_obj_t * segment_root, uint32_t seg_id, float seg_base_z, void * user)
{
    LV_UNUSED(user);
    const uint32_t seed = seg_id * 1103515245u + 12345u;

    for(int side = 0; side < 2; side++) {
        const float x_sign = side ? 1.0f : -1.0f;
        const int base_count = 3 + (int)((seed >> (side * 4)) & 3u);
        const int count = base_count * LVGL_S2_BUILDING_MUL;
        const float z_step = (S2_SEGMENT_LENGTH * 0.92f) / (float)(count > 0 ? count : 1);
        for(int i = 0; i < count; i++) {
            uint32_t h = seed + (uint32_t)(side * 17 + i * 31);
            float w = 60.0f + (float)(h % 50);
            float bh = 120.0f + (float)(h % 180);
            float d = 55.0f + (float)((h >> 3) % 40);
            float x = x_sign * (160.0f + (float)((h + (uint32_t)i * 13u) % 300));
            float z = seg_base_z - 16.0f - (float)i * z_step - (float)(h % 12);
            spawn_building(segment_root, x, bh * 0.5f, z, w, bh, d);
        }
    }

    spawn_road_segment(segment_root, seg_base_z);
}

static bool g_direct_gpu_present;
static volatile bool g_gpu_frame_pending;

bool lvgl_demos_direct_gpu_present(void)
{
    return g_direct_gpu_present;
}

void lvgl_demos_set_direct_gpu_present(bool enable)
{
    g_direct_gpu_present = enable;
}

bool lvgl_demos_skip_lv_refresh(void)
{
    return g_direct_gpu_present && lv_gpu_renderer_has_restorable_viewport();
}

void lvgl_demos_request_gpu_frame(void)
{
    g_gpu_frame_pending = true;
}

bool lvgl_demos_consume_gpu_frame(void)
{
    if(!g_gpu_frame_pending) return false;
    g_gpu_frame_pending = false;
    return true;
}

bool lvgl_demos_gpu_frame_pending(void)
{
    return g_gpu_frame_pending;
}

void lvgl_demos_pre_refresh(uint32_t elapsed_ms)
{
    if(!g_pool || elapsed_ms == 0) return;
    lv_3d_segment_pool_tick(g_pool, (float)elapsed_ms / 1000.0f);
}

static uint32_t s_pool_last_ms;

static void pool_timer_cb(lv_timer_t * t)
{
    LV_UNUSED(t);
    if(!g_pool) return;

    uint32_t now = lv_tick_get();
    float dt = 0.016f;
    if(s_pool_last_ms != 0) {
        dt = (float)(now - s_pool_last_ms) / 1000.0f;
        if(dt < 0.001f) return;
        if(dt > 0.25f) dt = 0.25f;
    }
    s_pool_last_ms = now;
    lv_3d_segment_pool_tick(g_pool, dt);
}

static void nav_hud_create(lv_obj_t * scr)
{
    lv_obj_t * hint = lv_label_create(scr);
    lv_label_set_text(hint, "NAV");
    lv_obj_set_style_text_color(hint, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(hint, lv_color_hex(0x0D47A1), 0);
    lv_obj_set_style_bg_opa(hint, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(hint, 12, 0);
    lv_obj_set_style_pad_ver(hint, 4, 0);
    lv_obj_set_style_radius(hint, 6, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_move_foreground(hint);
}

void lvgl_scenario2_skyline_create(void)
{
#if LV_USE_DRAW_GPU_RENDERER
    lv_gpu_renderer_set_ui_mode(LV_GPU_RENDERER_UI_NAV_AR);
#endif
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_opa(scr, LV_OPA_0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * cam = lv_3dcamera_create(scr);
    lv_3dcamera_set_perspective(cam, 50.0f, 10.0f, 8000.0f);
    lv_3dcamera_look_at(cam,
                        (lv_vec3_t) { 0, 320, 1100 },
                        (lv_vec3_t) { 0, 60, 0 },
                        (lv_vec3_t) { 0, 1, 0 });

    lv_obj_t * scene = lv_3dscene_create(scr);
    lv_obj_t * bake_root = lv_obj_create(scr);
    lv_obj_add_flag(bake_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t * vp = lv_3dviewport_create(scr);
    lv_obj_set_size(vp, LV_PCT(100), LV_PCT(100));
    lv_3dviewport_set_camera(vp, cam);
    lv_3dviewport_set_scene(vp, scene);

    lv_3d_segment_pool_cfg_t cfg = {
        .segment_length = 420.0f,
        .scroll_speed = 220.0f,
        .pool_size = 8,
        .recycle_z = 1350.0f,
    };
    const char * spd_env = getenv("LVGL_SCENARIO2_SCROLL_SPEED");
    if(spd_env && spd_env[0]) cfg.scroll_speed = (float)atof(spd_env);
    g_pool = lv_3d_segment_pool_create(scene, &cfg);
    lv_3d_segment_pool_set_build_fn(g_pool, s2_build_segment, NULL);

    spawn_nav_car_plane(scene, bake_root);
    nav_hud_create(scr);
    lv_timer_create(pool_timer_cb, 16, NULL);
}

uint32_t lvgl_scenario2_get_recycle_count(void)
{
    if(!g_pool) return 0;
    lv_3d_segment_pool_stats_t st;
    lv_3d_segment_pool_get_stats(g_pool, &st);
    return st.recycle_count;
}

float lvgl_scenario2_get_min_seg_z(void)
{
    if(!g_pool) return 0.0f;
    lv_3d_segment_pool_stats_t st;
    lv_3d_segment_pool_get_stats(g_pool, &st);
    return st.min_seg_z;
}

#else

bool lvgl_demos_direct_gpu_present(void)
{
    return false;
}

void lvgl_demos_set_direct_gpu_present(bool enable)
{
    LV_UNUSED(enable);
}

bool lvgl_demos_skip_lv_refresh(void)
{
    return false;
}

void lvgl_demos_request_gpu_frame(void)
{
}

bool lvgl_demos_consume_gpu_frame(void)
{
    return true;
}

bool lvgl_demos_gpu_frame_pending(void)
{
    return false;
}

void lvgl_demos_pre_refresh(uint32_t elapsed_ms)
{
    LV_UNUSED(elapsed_ms);
}

void lvgl_scenario2_skyline_create(void) {}

uint32_t lvgl_scenario2_get_recycle_count(void)
{
    return 0;
}

float lvgl_scenario2_get_min_seg_z(void)
{
    return 0.0f;
}

#endif
