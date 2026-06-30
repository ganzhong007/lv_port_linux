/**
 * @file lvgl_scenario3.c — 场景三：GPU 线框涡旋压力测试（CPU 软渲染会卡死，GPU 路径应流畅）
 */

#include "lvgl/lvgl.h"
#include "lvgl_demos.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS && LV_USE_DRAW_GPU_COMPOSITE

#include "draw/gpu_composite/lv_draw_gpu_composite.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#ifndef LVGL_S3_MESH_COUNT
    #define LVGL_S3_MESH_COUNT 10000
#endif

#ifndef LVGL_S3_ANIM_MS
    #define LVGL_S3_ANIM_MS 16
#endif

#define S3_MESH_MAX 250

typedef struct {
    lv_obj_t * mesh;
    float orbit_r;
    float orbit_phase;
    float spin_rate;
    float base_y;
    float base_z;
    float box_w;
    float box_h;
    float box_d;
} lvgl_s3_cell_t;

static lvgl_s3_cell_t g_cells[S3_MESH_MAX];
static int g_mesh_count;
static lv_obj_t * g_vp;
static lv_obj_t * g_cam;
static lv_obj_t * g_stats_label;
static lv_timer_t * g_anim_timer;
static float g_time;
static uint32_t g_hud_tick;
static char g_renderer_cached[128];

static int env_int(const char * name, int default_val)
{
    const char * v = getenv(name);
    if(!v || !v[0]) return default_val;
    return atoi(v);
}

static lv_color_t color_for_index(int i)
{
    static const uint32_t palette[] = {
        0x00E5FF, 0x18FFFF, 0x64FFDA, 0x69F0AE, 0xB2FF59,
        0xEEFF41, 0xFFFF00, 0xFFD740, 0xFFAB40, 0xFF6E40,
        0xFF5252, 0xFF4081, 0xE040FB, 0x7C4DFF, 0x536DFE,
        0x448AFF,
    };
    return lv_color_hex(palette[i % (int)(sizeof(palette) / sizeof(palette[0]))]);
}

static void spawn_wire_box(lv_obj_t * scene, lvgl_s3_cell_t * cell, int idx)
{
    const float u = (float)idx / (float)g_mesh_count * (float)(M_PI * 2.0);
    const float v = u * 4.2f;
    const float major_r = 340.0f;
    const float minor_r = 95.0f + (float)(idx % 13) * 14.0f;
    const float ring_x = (major_r + minor_r * cosf(v)) * cosf(u);
    const float ring_z = -220.0f - (float)(idx / 10) * 105.0f
                         - (major_r + minor_r * cosf(v)) * sinf(u) * 0.22f;

    cell->orbit_r = 28.0f + (float)(idx % 9) * 11.0f;
    cell->orbit_phase = u + (float)(idx % 17) * 0.37f;
    cell->spin_rate = 0.55f + (float)(idx % 7) * 0.12f;
    cell->base_y = 70.0f + minor_r * sinf(v) * 0.45f + (float)(idx % 5) * 18.0f;
    cell->base_z = ring_z;
    cell->box_w = 36.0f + (float)(idx % 11) * 8.0f;
    cell->box_h = 48.0f + (float)(idx % 9) * 10.0f;
    cell->box_d = 32.0f + (float)(idx % 7) * 7.0f;

    cell->mesh = lv_3dmesh_create(scene);
    lv_3dmesh_set_box(cell->mesh, cell->box_w, cell->box_h, cell->box_d);
    lv_3dmesh_set_wireframe(cell->mesh, true);
    lv_3dmesh_set_color(cell->mesh, color_for_index(idx));
    lv_3dmesh_set_position(cell->mesh, ring_x, cell->base_y, cell->base_z);
}

static void hud_create(lv_obj_t * scr, int mesh_count)
{
    lv_obj_t * panel = lv_obj_create(scr);
    lv_obj_set_size(panel, 420, 168);
    lv_obj_align(panel, LV_ALIGN_TOP_LEFT, 16, 16);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x0A0E17), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_80, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_pad_all(panel, 12, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * title = lv_label_create(panel);
    lv_label_set_text(title, "Scenario 3 — GPU Wireframe Vortex");
    lv_obj_set_style_text_color(title, lv_color_hex(0xE0F7FA), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    g_stats_label = lv_label_create(panel);
    lv_label_set_text_fmt(g_stats_label,
                          "Meshes: %d  (anim @ %d ms)\n"
                          "GPU stats updating…",
                          mesh_count, LVGL_S3_ANIM_MS);
    lv_obj_set_style_text_color(g_stats_label, lv_color_hex(0x80DEEA), 0);
    lv_obj_set_width(g_stats_label, 380);
    lv_obj_align(g_stats_label, LV_ALIGN_TOP_LEFT, 0, 28);

    lv_obj_t * hint = lv_label_create(scr);
    lv_label_set_text(hint,
                     "Heavy 3D wireframe animation — expect smooth playback on GPU composite backend.\n"
                     "Watch gpu_3d_draws / GL_RENDERER below, or run with LVGL_VERIFY=1.");
    lv_obj_set_style_text_color(hint, lv_color_hex(0xB0BEC5), 0);
    lv_obj_set_style_bg_color(hint, lv_color_hex(0x0A0E17), 0);
    lv_obj_set_style_bg_opa(hint, LV_OPA_70, 0);
    lv_obj_set_style_pad_hor(hint, 10, 0);
    lv_obj_set_style_pad_ver(hint, 6, 0);
    lv_obj_set_style_radius(hint, 6, 0);
    lv_obj_set_width(hint, LV_PCT(92));
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -12);
}

static void hud_update_path_stats(void)
{
    if(!g_stats_label) return;

    lv_gpu_composite_path_stats_t st;
    lv_gpu_composite_get_path_stats(&st);
    if(st.gl_renderer[0]) {
        lv_strncpy(g_renderer_cached, st.gl_renderer, sizeof(g_renderer_cached) - 1);
        g_renderer_cached[sizeof(g_renderer_cached) - 1] = '\0';
    }
    const char * renderer = g_renderer_cached[0] ? g_renderer_cached : "(probing…)";
    lv_label_set_text_fmt(g_stats_label,
                          "Meshes: %d  (anim @ %d ms)\n"
                          "gpu_3d_draws: %u  flush_items: %u\n"
                          "gpu_2d_tasks: %u  sw_raster: %u\n"
                          "GL_RENDERER: %s",
                          g_mesh_count, LVGL_S3_ANIM_MS,
                          (unsigned)st.gpu_3d_draws,
                          (unsigned)st.last_flush_items,
                          (unsigned)st.gpu_2d_tasks,
                          (unsigned)st.sw_2d_raster_tasks,
                          renderer);
}

static void anim_cb(lv_timer_t * t)
{
    LV_UNUSED(t);
    g_time += (float)LVGL_S3_ANIM_MS / 1000.0f;

    const float tunnel_rot = g_time * 42.0f;
    const float scroll = fmodf(g_time * 320.0f, 2800.0f);
    const float cam_a = g_time * 0.22f;

    for(int i = 0; i < g_mesh_count; i++) {
        lvgl_s3_cell_t * c = &g_cells[i];
        const float a = c->orbit_phase + tunnel_rot * c->spin_rate * 0.017453292f;
        float x = cosf(a) * c->orbit_r;
        float y = c->base_y + sinf(a * 1.7f) * c->orbit_r * 0.38f;
        float z = c->base_z + scroll;
        while(z > 350.0f) z -= 2800.0f;

        const float pitch = g_time * 95.0f + (float)i * 11.0f;
        const float yaw = tunnel_rot * c->spin_rate + (float)i * 19.0f;
        const float roll = sinf(g_time * 1.4f + (float)i * 0.31f) * 38.0f;

        lv_3dmesh_set_position(c->mesh, x, y, z);
        lv_3dmesh_set_rotation(c->mesh, pitch, yaw, roll);
    }

    if(g_cam) {
        lv_3dcamera_look_at(g_cam,
                            (lv_vec3_t) {
                                sinf(cam_a) * 140.0f,
                                260.0f + sinf(g_time * 0.55f) * 55.0f,
                                920.0f + cosf(cam_a) * 110.0f
                            },
                            (lv_vec3_t) { 0.0f, 90.0f, -180.0f },
                            (lv_vec3_t) { 0.0f, 1.0f, 0.0f });
    }

    if(g_vp) lv_obj_invalidate(g_vp);

    g_hud_tick++;
    if(g_hud_tick >= 30) {
        g_hud_tick = 0;
        hud_update_path_stats();
    }
}

void lvgl_scenario3_gpu_stress_create(void)
{
#if LV_USE_DRAW_GPU_COMPOSITE
    lv_gpu_composite_set_ui_mode(LV_GPU_COMPOSITE_UI_GENERIC);
#endif
    g_mesh_count = env_int("LVGL_S3_MESH_COUNT", LVGL_S3_MESH_COUNT);
    if(g_mesh_count < 16) g_mesh_count = 16;
    if(g_mesh_count > S3_MESH_MAX) g_mesh_count = S3_MESH_MAX;

    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_opa(scr, LV_OPA_0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    g_cam = lv_3dcamera_create(scr);
    lv_3dcamera_set_perspective(g_cam, 52.0f, 10.0f, 9000.0f);
    lv_3dcamera_look_at(g_cam,
                        (lv_vec3_t) { 0.0f, 280.0f, 950.0f },
                        (lv_vec3_t) { 0.0f, 80.0f, -200.0f },
                        (lv_vec3_t) { 0.0f, 1.0f, 0.0f });

    lv_obj_t * scene = lv_3dscene_create(scr);
    g_vp = lv_3dviewport_create(scr);
    lv_obj_set_size(g_vp, LV_PCT(100), LV_PCT(100));
    lv_3dviewport_set_camera(g_vp, g_cam);
    lv_3dviewport_set_scene(g_vp, scene);

    for(int i = 0; i < g_mesh_count; i++) {
        spawn_wire_box(scene, &g_cells[i], i);
    }

    hud_create(scr, g_mesh_count);
    g_time = 0.0f;
    g_hud_tick = 0;
    g_renderer_cached[0] = '\0';
    g_anim_timer = lv_timer_create(anim_cb, LVGL_S3_ANIM_MS, NULL);

    printf("LVGL_SCENARIO3: mesh_count=%d anim_ms=%d (set LVGL_S3_MESH_COUNT to tune load)\n",
           g_mesh_count, LVGL_S3_ANIM_MS);
}

int lvgl_scenario3_get_mesh_count(void)
{
    return g_mesh_count;
}

#else

void lvgl_scenario3_gpu_stress_create(void)
{
    printf("LVGL_SCENARIO3: requires LV_USE_3D + LV_USE_DRAW_GPU_COMPOSITE\n");
}

int lvgl_scenario3_get_mesh_count(void)
{
    return 0;
}

#endif
