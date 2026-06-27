/**
 * @file lvgl_scenario1.c — 场景一：9 宫格 + 左右侧翼预览 + 触控全屏动效
 */

#include "lvgl/lvgl.h"
#include "lvgl_demos.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS && LV_USE_3DSTACK

#include <math.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#define LVGL_S1_TILE_COUNT 11
#define LVGL_S1_ANIM_MS      420

typedef struct {
    lv_obj_t * mesh;
    float home_pos[3];
    float home_rot_y;  /* degrees */
    float home_scale[3];
    lv_color_t color;
} lvgl_s1_tile_t;

static lvgl_s1_tile_t g_tiles[LVGL_S1_TILE_COUNT];
static lv_obj_t * g_vp;
static int g_focused = -1;
static bool g_animating;
static lv_timer_t * g_anim_timer;

static int s_anim_target = -1;
static uint32_t s_anim_start;
static float s_from_pos[LVGL_S1_TILE_COUNT][3];
static float s_from_scale[LVGL_S1_TILE_COUNT][3];
static float s_from_rot_y[LVGL_S1_TILE_COUNT];
static lv_opa_t s_from_opa[LVGL_S1_TILE_COUNT];

static const uint32_t tile_colors[LVGL_S1_TILE_COUNT] = {
    0xE53935, 0xFB8C00, 0xFDD835, 0x43A047, 0x1E88E5,
    0x8E24AA, 0x00ACC1, 0x6D4C41, 0x546E7A,
    0x26A69A, 0xEC407A,
};

static float ease_out_cubic(float t)
{
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

static void tile_apply(lvgl_s1_tile_t * t, float x, float y, float z,
                       float yaw_deg, float sx, float sy, float sz, lv_opa_t opa)
{
    lv_3d_material_t mat;
    lv_3d_material_init(&mat, LV_3D_MAT_OPAQUE, t->color, opa);
    lv_3dmesh_set_material(t->mesh, &mat);
    lv_3dmesh_set_position(t->mesh, x, y, z);
    lv_3dmesh_set_rotation_y(t->mesh, yaw_deg);
    lv_3dmesh_set_scale(t->mesh, sx, sy, sz);
}

static void tile_apply_home(int idx, lv_opa_t opa)
{
    lvgl_s1_tile_t * t = &g_tiles[idx];
    tile_apply(t,
               t->home_pos[0], t->home_pos[1], t->home_pos[2],
               t->home_rot_y,
               t->home_scale[0], t->home_scale[1], t->home_scale[2],
               opa);
}

static void anim_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);

    uint32_t elapsed = lv_tick_elaps(s_anim_start);
    float u = (float)elapsed / (float)LVGL_S1_ANIM_MS;
    if(u >= 1.0f) {
        u = 1.0f;
        g_animating = false;
        g_focused = s_anim_target;
        if(g_anim_timer) {
            lv_timer_delete(g_anim_timer);
            g_anim_timer = NULL;
        }
    }

    float e = ease_out_cubic(u);

    for(int i = 0; i < LVGL_S1_TILE_COUNT; i++) {
        lvgl_s1_tile_t * t = &g_tiles[i];
        float x, y, z, sx, sy, sz, yaw;
        lv_opa_t opa;

        if(s_anim_target >= 0 && i == s_anim_target) {
            x = s_from_pos[i][0] + (0.0f - s_from_pos[i][0]) * e;
            y = s_from_pos[i][1] + (0.0f - s_from_pos[i][1]) * e;
            z = s_from_pos[i][2] + (-220.0f - s_from_pos[i][2]) * e;
            sx = s_from_scale[i][0] + (3.2f - s_from_scale[i][0]) * e;
            sy = s_from_scale[i][1] + (3.2f - s_from_scale[i][1]) * e;
            sz = s_from_scale[i][2] + (3.2f - s_from_scale[i][2]) * e;
            yaw = s_from_rot_y[i] + (0.0f - s_from_rot_y[i]) * e;
            opa = (lv_opa_t)(s_from_opa[i] + ((int32_t)LV_OPA_COVER - s_from_opa[i]) * e);
        }
        else if(s_anim_target >= 0) {
            x = s_from_pos[i][0];
            y = s_from_pos[i][1];
            z = s_from_pos[i][2] + (-520.0f) * e;
            sx = s_from_scale[i][0] * (1.0f - 0.55f * e);
            sy = s_from_scale[i][1] * (1.0f - 0.55f * e);
            sz = s_from_scale[i][2] * (1.0f - 0.55f * e);
            yaw = s_from_rot_y[i];
            opa = (lv_opa_t)(s_from_opa[i] * (1.0f - e));
        }
        else {
            x = s_from_pos[i][0] + (t->home_pos[0] - s_from_pos[i][0]) * e;
            y = s_from_pos[i][1] + (t->home_pos[1] - s_from_pos[i][1]) * e;
            z = s_from_pos[i][2] + (t->home_pos[2] - s_from_pos[i][2]) * e;
            sx = s_from_scale[i][0] + (t->home_scale[0] - s_from_scale[i][0]) * e;
            sy = s_from_scale[i][1] + (t->home_scale[1] - s_from_scale[i][1]) * e;
            sz = s_from_scale[i][2] + (t->home_scale[2] - s_from_scale[i][2]) * e;
            yaw = s_from_rot_y[i] + (t->home_rot_y - s_from_rot_y[i]) * e;
            opa = (lv_opa_t)(s_from_opa[i] + ((int32_t)LV_OPA_COVER - s_from_opa[i]) * e);
        }

        tile_apply(t, x, y, z, yaw, sx, sy, sz, opa);
    }

    if(g_vp) lv_obj_invalidate(g_vp);
}

static void snapshot_from_current(void)
{
    for(int i = 0; i < LVGL_S1_TILE_COUNT; i++) {
        lvgl_s1_tile_t * t = &g_tiles[i];
        if(g_focused >= 0 && i == g_focused) {
            s_from_pos[i][0] = 0.0f;
            s_from_pos[i][1] = 0.0f;
            s_from_pos[i][2] = -220.0f;
            s_from_scale[i][0] = s_from_scale[i][1] = s_from_scale[i][2] = 3.2f;
            s_from_rot_y[i] = 0.0f;
            s_from_opa[i] = LV_OPA_COVER;
        }
        else {
            s_from_pos[i][0] = t->home_pos[0];
            s_from_pos[i][1] = t->home_pos[1];
            s_from_pos[i][2] = t->home_pos[2];
            s_from_scale[i][0] = t->home_scale[0];
            s_from_scale[i][1] = t->home_scale[1];
            s_from_scale[i][2] = t->home_scale[2];
            s_from_rot_y[i] = t->home_rot_y;
            s_from_opa[i] = (g_focused >= 0 && i != g_focused) ? LV_OPA_0 : LV_OPA_COVER;
        }
    }
}

static void start_anim(int target_idx)
{
    if(g_animating) return;

    s_anim_target = target_idx;
    s_anim_start = lv_tick_get();
    snapshot_from_current();

    g_animating = true;
    if(g_anim_timer) lv_timer_delete(g_anim_timer);
    g_anim_timer = lv_timer_create(anim_timer_cb, 16, NULL);
}

static int pick_tile_from_screen(int32_t x, int32_t y, int32_t w, int32_t h)
{
    if(w < 1 || h < 1) return -1;

    if(x < w / 9) return 9;
    if(x > w * 8 / 9) return 10;

    int col = (x * 3) / w;
    int row = (y * 3) / h;
    if(col < 0) col = 0;
    if(col > 2) col = 2;
    if(row < 0) row = 0;
    if(row > 2) row = 2;
    return row * 3 + col;
}

static void vp_click_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(g_animating) return;

    lv_indev_t * indev = lv_indev_active();
    if(!indev) return;

    lv_point_t pt;
    lv_indev_get_point(indev, &pt);

    int32_t w = lv_obj_get_width(g_vp);
    int32_t h = lv_obj_get_height(g_vp);
    int idx = pick_tile_from_screen(pt.x, pt.y, w, h);
    if(idx < 0) return;

    if(g_focused < 0) {
        start_anim(idx);
    }
    else if(idx == g_focused) {
        start_anim(-1);
    }
    else {
        g_focused = -1;
        for(int i = 0; i < LVGL_S1_TILE_COUNT; i++) tile_apply_home(i, LV_OPA_COVER);
        start_anim(idx);
    }
}

static void setup_peek_tiles(lv_obj_t * scene, float x_gap, float row_z0)
{
    const float peek_w = 44.0f;
    const float peek_h = 112.0f;
    const float peek_x = x_gap * 2.05f;
    const float yaw_deg = 22.0f;

    lv_obj_t * peek_l = lv_3dmesh_create(scene);
    lv_3dmesh_set_box(peek_l, peek_w, peek_h, 8);
    lv_3dmesh_set_wireframe(peek_l, false);
    {
        lv_3d_material_t mat;
        lv_3d_material_init(&mat, LV_3D_MAT_OPAQUE, lv_color_hex(tile_colors[9]), LV_OPA_COVER);
        lv_3dmesh_set_material(peek_l, &mat);
    }
    lv_3dmesh_set_position(peek_l, -peek_x, 0.0f, row_z0);
    lv_3dmesh_set_rotation_y(peek_l, yaw_deg);
    lv_3dmesh_set_scale(peek_l, 0.92f, 0.92f, 0.92f);
    g_tiles[9].mesh = peek_l;
    g_tiles[9].color = lv_color_hex(tile_colors[9]);
    g_tiles[9].home_pos[0] = -peek_x;
    g_tiles[9].home_pos[1] = 0.0f;
    g_tiles[9].home_pos[2] = row_z0;
    g_tiles[9].home_rot_y = yaw_deg;
    g_tiles[9].home_scale[0] = g_tiles[9].home_scale[1] = g_tiles[9].home_scale[2] = 0.92f;

    lv_obj_t * peek_r = lv_3dmesh_create(scene);
    lv_3dmesh_set_box(peek_r, peek_w, peek_h, 8);
    lv_3dmesh_set_wireframe(peek_r, false);
    {
        lv_3d_material_t mat;
        lv_3d_material_init(&mat, LV_3D_MAT_OPAQUE, lv_color_hex(tile_colors[10]), LV_OPA_COVER);
        lv_3dmesh_set_material(peek_r, &mat);
    }
    lv_3dmesh_set_position(peek_r, peek_x, 0.0f, row_z0);
    lv_3dmesh_set_rotation_y(peek_r, -yaw_deg);
    lv_3dmesh_set_scale(peek_r, 0.92f, 0.92f, 0.92f);
    g_tiles[10].mesh = peek_r;
    g_tiles[10].color = lv_color_hex(tile_colors[10]);
    g_tiles[10].home_pos[0] = peek_x;
    g_tiles[10].home_pos[1] = 0.0f;
    g_tiles[10].home_pos[2] = row_z0;
    g_tiles[10].home_rot_y = -yaw_deg;
    g_tiles[10].home_scale[0] = g_tiles[10].home_scale[1] = g_tiles[10].home_scale[2] = 0.92f;
}

static void capture_grid_homes(lv_obj_t * stack)
{
    const float x_gap = 220.0f;
    const float y_gap = 160.0f;
    const float row_z[3] = { -350.0f, -650.0f, -950.0f };

    for(uint32_t i = 0; i < 9; i++) {
        lv_obj_t * tile = lv_obj_get_child(stack, i);
        uint32_t r = i / 3;
        uint32_t c = i % 3;
        float cx = (float)c - 1.0f;
        float cy = (float)r - 1.0f;
        float sc = 1.0f - (float)r * 0.08f;

        g_tiles[i].mesh = tile;
        g_tiles[i].color = lv_color_hex(tile_colors[i]);
        g_tiles[i].home_pos[0] = cx * x_gap;
        g_tiles[i].home_pos[1] = cy * y_gap;
        g_tiles[i].home_pos[2] = row_z[r];
        g_tiles[i].home_scale[0] = g_tiles[i].home_scale[1] = g_tiles[i].home_scale[2] = sc;
        g_tiles[i].home_rot_y = 0.0f;
    }
}

void lvgl_scenario1_launcher_create(void)
{
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_opa(scr, LV_OPA_0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * cam = lv_3dcamera_create(scr);
    lv_3dcamera_set_perspective(cam, 42.0f, 10.0f, 6000.0f);
    lv_3dcamera_look_at(cam,
                        (lv_vec3_t) { 0, 120, 900 },
                        (lv_vec3_t) { 0, 0, -400 },
                        (lv_vec3_t) { 0, 1, 0 });

    lv_obj_t * scene = lv_3dscene_create(scr);
    lv_obj_t * stack = lv_3dstack_create(scene);
    lv_3dstack_set_grid(stack, 3, 3);
    lv_3dstack_set_row_depth(stack, 0, -350);
    lv_3dstack_set_row_depth(stack, 1, -650);
    lv_3dstack_set_row_depth(stack, 2, -950);
    lv_3dstack_set_cell_spacing(stack, 220, 160);

    for(int i = 0; i < 9; i++) {
        lv_obj_t * tile = lv_3dmesh_create(stack);
        lv_3dmesh_set_box(tile, 160, 100, 8);
        lv_3dmesh_set_wireframe(tile, false);
        lv_3d_material_t mat;
        lv_3d_material_init(&mat, LV_3D_MAT_OPAQUE, lv_color_hex(tile_colors[i]), LV_OPA_COVER);
        lv_3dmesh_set_material(tile, &mat);
    }

    lv_3dstack_layout(stack);
    capture_grid_homes(stack);
    setup_peek_tiles(scene, 220.0f, -350.0f);

    g_vp = lv_3dviewport_create(scr);
    lv_obj_set_size(g_vp, LV_PCT(100), LV_PCT(100));
    lv_3dviewport_set_camera(g_vp, cam);
    lv_3dviewport_set_scene(g_vp, scene);
    lv_obj_add_flag(g_vp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_vp, vp_click_cb, LV_EVENT_CLICKED, NULL);

    g_focused = -1;
    g_animating = false;
    g_anim_timer = NULL;
}

#else

void lvgl_scenario1_launcher_create(void) {}

#endif
