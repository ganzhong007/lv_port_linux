/**
 * @file lvgl_scenario2.c — 场景二：导航车道 + 两侧楼群 parallax 后移 (Phase 2)
 */

#include "lvgl/lvgl.h"
#include "lvgl_demos.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS && LV_USE_3D_SEGMENT_POOL

#include <stdlib.h>

/** Buildings per side = (3..6) * this factor (default 1). */
#ifndef LVGL_S2_BUILDING_MUL
    #define LVGL_S2_BUILDING_MUL 1
#endif
#define S2_SEGMENT_LENGTH 420.0f

static lv_3d_segment_pool_t * g_pool;

static void spawn_building(lv_obj_t * root, float x, float y, float z, float w, float h, float d, lv_color_t c)
{
    lv_obj_t * m = lv_3dmesh_create(root);
    lv_3dmesh_set_box(m, w, h, d);
    lv_3dmesh_set_wireframe(m, true);
    lv_3dmesh_set_color(m, c);
    lv_3dmesh_set_position(m, x, y, z);
}

static void s2_build_segment(lv_obj_t * segment_root, uint32_t seg_id, float seg_base_z, void * user)
{
    LV_UNUSED(user);
    const uint32_t seed = seg_id * 1103515245u + 12345u;
    const lv_color_t colors[] = {
        lv_color_hex(0x00FFAA),
        lv_color_hex(0x26C6DA),
        lv_color_hex(0x66BB6A),
        lv_color_hex(0xAB47BC),
    };

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
            lv_color_t c = colors[(h >> 5) % 4];
            spawn_building(segment_root, x, bh * 0.5f, z, w, bh, d, c);
        }
    }
}

static void pool_timer_cb(lv_timer_t * t)
{
    LV_UNUSED(t);
    if(g_pool) lv_3d_segment_pool_tick(g_pool, 0.016f);
}

static void lane_overlay_create(lv_obj_t * scr)
{
    lv_obj_t * lane = lv_obj_create(scr);
    lv_obj_set_size(lane, 140, 300);
    lv_obj_align(lane, LV_ALIGN_BOTTOM_MID, 0, -36);
    lv_obj_set_style_bg_color(lane, lv_color_hex(0x1565C0), 0);
    lv_obj_set_style_bg_opa(lane, LV_OPA_70, 0);
    lv_obj_set_style_border_color(lane, lv_color_hex(0x42A5F5), 0);
    lv_obj_set_style_border_width(lane, 3, 0);
    lv_obj_set_style_radius(lane, 12, 0);
    lv_obj_remove_flag(lane, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * chevron = lv_label_create(lane);
    lv_label_set_text(chevron, LV_SYMBOL_UP "\n" LV_SYMBOL_UP "\n" LV_SYMBOL_UP);
    lv_obj_set_style_text_color(chevron, lv_color_hex(0xE3F2FD), 0);
    lv_obj_set_style_text_align(chevron, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(chevron);

    lv_obj_t * hint = lv_label_create(scr);
    lv_label_set_text(hint, "NAV");
    lv_obj_set_style_text_color(hint, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(hint, lv_color_hex(0x0D47A1), 0);
    lv_obj_set_style_bg_opa(hint, LV_OPA_80, 0);
    lv_obj_set_style_pad_hor(hint, 12, 0);
    lv_obj_set_style_pad_ver(hint, 4, 0);
    lv_obj_set_style_radius(hint, 6, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 24);
}

void lvgl_scenario2_skyline_create(void)
{
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
    lv_obj_t * vp = lv_3dviewport_create(scr);
    lv_obj_set_size(vp, LV_PCT(100), LV_PCT(100));
    lv_3dviewport_set_camera(vp, cam);
    lv_3dviewport_set_scene(vp, scene);

    lv_3d_segment_pool_cfg_t cfg = {
        .segment_length = 420.0f,
        .scroll_speed = 140.0f,
        .pool_size = 8,
        .recycle_z = 1350.0f,
    };
    const char * spd_env = getenv("LVGL_SCENARIO2_SCROLL_SPEED");
    if(spd_env && spd_env[0]) cfg.scroll_speed = (float)atof(spd_env);
    g_pool = lv_3d_segment_pool_create(scene, &cfg);
    lv_3d_segment_pool_set_build_fn(g_pool, s2_build_segment, NULL);

    lane_overlay_create(scr);
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
