/**
 * @file lvgl_scenario7.c — Scenario 7: Even UX side-view panels (native 3D, no video file).
 *
 * Replicates assets/clipped.mp4 (~20 s): alternating symmetric 2×2 grid and hero-focus
 * layouts (each digit 1–4 enlarges in center with one empty wireframe corner).
 */

#include "lvgl/lvgl.h"
#include "lvgl_demos.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS && LV_USE_DRAW_GPU_RENDERER

#include "draw/gpu_renderer/lv_draw_gpu_renderer.h"
#include "3d/lv_3d_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define S7_PANEL_W        150.0f
#define S7_PANEL_H        150.0f
#define S7_PANEL_DEPTH      8.0f
#define S7_ANIM_TOTAL_MS   20000

typedef enum {
    S7_SLOT_TL = 0,
    S7_SLOT_BL,
    S7_SLOT_TR,
    S7_SLOT_BR,
    S7_SLOT_CENTER,
    S7_SLOT_HIDDEN,
    S7_SLOT_COUNT
} s7_slot_t;

typedef struct {
    lv_obj_t * fill;
    lv_obj_t * wire;
} s7_panel_t;

typedef struct {
    float x, y, z;
    float yaw_deg;
    float scale;
} s7_pose_t;

typedef struct {
    s7_pose_t panels[4];
    bool fill_vis[4];
    s7_pose_t empty_wire;
    bool empty_vis;
} s7_layout_t;

typedef struct {
    uint32_t t_ms;
    bool grid;
    int hero;          /* 1..4 when !grid */
    s7_slot_t empty;   /* wire-only corner when !grid */
} s7_keyframe_t;

static lv_obj_t * g_vp;
static lv_obj_t * g_scene;
static s7_panel_t g_panels[4];
static lv_obj_t * g_empty_wire;
static uint32_t g_anim_start;
static lv_timer_t * g_anim_timer;

static const s7_pose_t g_slot_pose[S7_SLOT_COUNT] = {
    [S7_SLOT_TL]     = { -230.0f,  88.0f, -380.0f,  38.0f, 1.0f },
    [S7_SLOT_BL]     = { -230.0f, -88.0f, -380.0f,  38.0f, 1.0f },
    [S7_SLOT_TR]     = {  230.0f,  88.0f, -380.0f, -38.0f, 1.0f },
    [S7_SLOT_BR]     = {  230.0f, -88.0f, -380.0f, -38.0f, 1.0f },
    [S7_SLOT_CENTER] = {    0.0f,   0.0f, -400.0f,   0.0f, 1.85f },
    [S7_SLOT_HIDDEN] = {    0.0f,   0.0f, -900.0f,   0.0f, 0.01f },
};

/** Keyframes sampled from clipped.mp4 (~20 s). */
static const s7_keyframe_t g_keyframes[] = {
    {     0, true,  0, S7_SLOT_TR },
    {  1200, true,  0, S7_SLOT_TR },
    {  2800, false, 3, S7_SLOT_TR },
    {  3800, false, 3, S7_SLOT_TR },
    {  4200, true,  0, S7_SLOT_TR },
    {  4800, false, 3, S7_SLOT_TR },
    {  5400, true,  0, S7_SLOT_TR },
    {  6800, false, 1, S7_SLOT_TL },
    {  7600, true,  0, S7_SLOT_TR },
    {  8200, false, 1, S7_SLOT_TL },
    {  9200, true,  0, S7_SLOT_TR },
    { 11200, false, 1, S7_SLOT_TL },
    { 12800, true,  0, S7_SLOT_TR },
    { 13800, false, 4, S7_SLOT_BR },
    { 15200, false, 3, S7_SLOT_TR },
    { 16800, false, 3, S7_SLOT_TR },
    { 18200, true,  0, S7_SLOT_TR },
    { 20000, true,  0, S7_SLOT_TR },
};

#define S7_KEYFRAME_COUNT ((int)(sizeof(g_keyframes) / sizeof(g_keyframes[0])))

static float s7_ease_in_out(float t)
{
    if(t <= 0.0f) return 0.0f;
    if(t >= 1.0f) return 1.0f;
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static void s7_pose_lerp(const s7_pose_t * a, const s7_pose_t * b, float t, s7_pose_t * out)
{
    out->x = a->x + (b->x - a->x) * t;
    out->y = a->y + (b->y - a->y) * t;
    out->z = a->z + (b->z - a->z) * t;
    out->yaw_deg = a->yaw_deg + (b->yaw_deg - a->yaw_deg) * t;
    out->scale = a->scale + (b->scale - a->scale) * t;
}

static void s7_layout_grid(s7_layout_t * out)
{
    static const s7_slot_t map[4] = { S7_SLOT_TL, S7_SLOT_BL, S7_SLOT_TR, S7_SLOT_BR };
    memset(out, 0, sizeof(*out));
    for(int i = 0; i < 4; i++) {
        out->panels[i] = g_slot_pose[map[i]];
        out->fill_vis[i] = true;
    }
    out->empty_vis = false;
}

static void s7_layout_hero(int hero, s7_slot_t empty, s7_layout_t * out)
{
    static const s7_slot_t corners[4] = { S7_SLOT_TL, S7_SLOT_BL, S7_SLOT_TR, S7_SLOT_BR };
    s7_slot_t assign[4];

    memset(out, 0, sizeof(*out));
    for(int i = 0; i < 4; i++) assign[i] = S7_SLOT_HIDDEN;

    if(hero >= 1 && hero <= 4) assign[hero - 1] = S7_SLOT_CENTER;

    int rem[3];
    int rn = 0;
    for(int d = 1; d <= 4; d++) {
        if(d != hero) rem[rn++] = d;
    }

    int ri = 0;
    for(int c = 0; c < 4; c++) {
        if(corners[c] == empty) continue;
        if(ri < rn) assign[rem[ri++] - 1] = corners[c];
    }

    for(int i = 0; i < 4; i++) {
        out->panels[i] = g_slot_pose[assign[i]];
        out->fill_vis[i] = (assign[i] != S7_SLOT_HIDDEN);
    }

    out->empty_wire = g_slot_pose[empty];
    out->empty_vis = true;
}

static void s7_layout_from_keyframe(const s7_keyframe_t * kf, s7_layout_t * out)
{
    if(kf->grid) s7_layout_grid(out);
    else s7_layout_hero(kf->hero, kf->empty, out);
}

static void s7_layout_blend(const s7_layout_t * a, const s7_layout_t * b, float t, s7_layout_t * out)
{
    for(int i = 0; i < 4; i++) {
        s7_pose_lerp(&a->panels[i], &b->panels[i], t, &out->panels[i]);
        out->fill_vis[i] = (t < 0.5f) ? a->fill_vis[i] : b->fill_vis[i];
    }
    s7_pose_lerp(&a->empty_wire, &b->empty_wire, t, &out->empty_wire);
    out->empty_vis = (t < 0.5f) ? a->empty_vis : b->empty_vis;
    if(t > 0.05f && t < 0.95f) out->empty_vis = a->empty_vis || b->empty_vis;
}

static void s7_sample_layout(uint32_t t_ms, s7_layout_t * out)
{
    const uint32_t t = t_ms % S7_ANIM_TOTAL_MS;
    int hi = 1;

    while(hi < S7_KEYFRAME_COUNT && g_keyframes[hi].t_ms <= t) hi++;
    if(hi >= S7_KEYFRAME_COUNT) hi = S7_KEYFRAME_COUNT - 1;

    const s7_keyframe_t * k0 = &g_keyframes[hi - 1];
    const s7_keyframe_t * k1 = &g_keyframes[hi];
    s7_layout_t a, b;

    s7_layout_from_keyframe(k0, &a);
    s7_layout_from_keyframe(k1, &b);

    if(k1->t_ms <= k0->t_ms || t >= k1->t_ms) {
        *out = b;
        return;
    }

    const float u = s7_ease_in_out((float)(t - k0->t_ms) / (float)(k1->t_ms - k0->t_ms));
    s7_layout_blend(&a, &b, u, out);
}

static void s7_apply_mesh_pose(lv_obj_t * fill, lv_obj_t * wire, const s7_pose_t * pose, bool fill_vis)
{
    const float sx = pose->scale;
    const float sy = pose->scale;
    const float sz = pose->scale * (pose->scale > 1.2f ? 1.15f : 1.0f);

    if(fill) {
        lv_3dmesh_set_position(fill, pose->x, pose->y, pose->z);
        lv_3dmesh_set_rotation_y(fill, pose->yaw_deg);
        lv_3dmesh_set_scale(fill, sx, sy, sz);
        lv_3dmesh_set_opa(fill, fill_vis ? LV_OPA_COVER : LV_OPA_TRANSP);
    }
    if(wire) {
        lv_3dmesh_set_position(wire, pose->x, pose->y, pose->z);
        lv_3dmesh_set_rotation_y(wire, pose->yaw_deg);
        lv_3dmesh_set_scale(wire, sx, sy, sz);
        lv_3dmesh_set_opa(wire, LV_OPA_COVER);
    }
}

static void s7_apply_layout(const s7_layout_t * layout)
{
    for(int i = 0; i < 4; i++) {
        s7_apply_mesh_pose(g_panels[i].fill, g_panels[i].wire, &layout->panels[i], layout->fill_vis[i]);
    }

    if(g_empty_wire) {
        if(layout->empty_vis) {
            s7_apply_mesh_pose(NULL, g_empty_wire, &layout->empty_wire, false);
        }
        else {
            lv_3dmesh_set_opa(g_empty_wire, LV_OPA_TRANSP);
        }
    }

    if(g_vp) {
        lv_3d_scene_mark_dirty(g_scene);
        lv_obj_invalidate(g_vp);
    }
}

static lv_obj_t * s7_bake_digit(lv_obj_t * root, const char * digit)
{
    lv_obj_t * tile = lv_obj_create(root);
    lv_obj_set_size(tile, 160, 160);
    lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(tile, lv_color_hex(0x0C3318), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_80, 0);
    lv_obj_set_style_radius(tile, 18, 0);
    lv_obj_set_style_border_color(tile, lv_color_hex(0x00FF55), 0);
    lv_obj_set_style_border_width(tile, 4, 0);
    lv_obj_set_style_border_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);

    lv_obj_t * lbl = lv_label_create(tile);
    lv_label_set_text(lbl, digit);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x00FF55), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_48, 0);
    lv_obj_center(lbl);

    return tile;
}

static void s7_panel_build(lv_obj_t * scene, lv_obj_t * bake_root, s7_panel_t * p, const char * digit)
{
    lv_obj_t * fill = lv_3dmesh_create(scene);
    lv_obj_t * wire = lv_3dmesh_create(scene);

    lv_obj_t * src = s7_bake_digit(bake_root, digit);
    lv_3d_snapshot_id_t snap = lv_3d_plane_bake(src, LV_3D_PLANE_SRC_SNAPSHOT);
    lv_3dmesh_set_box(fill, S7_PANEL_W, S7_PANEL_H, S7_PANEL_DEPTH);
    if(snap != LV_3D_SNAPSHOT_ID_NONE) {
        lv_3dmesh_set_plane_snapshot(fill, snap);
    }
    else {
        lv_3d_material_t mat;
        lv_3d_material_init(&mat, LV_3D_MAT_ALPHA, lv_color_hex(0x1B5E20), LV_OPA_70);
        lv_3dmesh_set_material(fill, &mat);
    }

    lv_3dmesh_set_box(wire, S7_PANEL_W, S7_PANEL_H, S7_PANEL_DEPTH);
    lv_3dmesh_set_wireframe(wire, true);
    lv_3d_material_t wf;
    lv_3d_material_init(&wf, LV_3D_MAT_WIREFRAME, lv_color_hex(0x00FF55), LV_OPA_COVER);
    lv_3dmesh_set_material(wire, &wf);

    p->fill = fill;
    p->wire = wire;
}

static void s7_anim_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    s7_layout_t layout;
    s7_sample_layout(lv_tick_elaps(g_anim_start), &layout);
    s7_apply_layout(&layout);
}

void lvgl_scenario7_video_crop_create(void)
{
    lv_gpu_renderer_set_ui_mode(LV_GPU_RENDERER_UI_GENERIC);

    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_opa(scr, LV_OPA_0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * cam = lv_3dcamera_create(scr);
    lv_3dcamera_set_perspective(cam, 38.0f, 10.0f, 5000.0f);
    lv_3dcamera_look_at(cam,
                        (lv_vec3_t) { 0.0f, 0.0f, 720.0f },
                        (lv_vec3_t) { 0.0f, 0.0f, -380.0f },
                        (lv_vec3_t) { 0.0f, 1.0f, 0.0f });

    g_scene = lv_3dscene_create(scr);
    lv_obj_set_style_bg_opa(g_scene, LV_OPA_0, 0);

    lv_obj_t * bake_root = lv_obj_create(scr);
    lv_obj_add_flag(bake_root, LV_OBJ_FLAG_HIDDEN);

    s7_panel_build(g_scene, bake_root, &g_panels[0], "1");
    s7_panel_build(g_scene, bake_root, &g_panels[1], "2");
    s7_panel_build(g_scene, bake_root, &g_panels[2], "3");
    s7_panel_build(g_scene, bake_root, &g_panels[3], "4");

    g_empty_wire = lv_3dmesh_create(g_scene);
    lv_3dmesh_set_box(g_empty_wire, S7_PANEL_W, S7_PANEL_H, S7_PANEL_DEPTH);
    lv_3dmesh_set_wireframe(g_empty_wire, true);
    lv_3d_material_t ew;
    lv_3d_material_init(&ew, LV_3D_MAT_WIREFRAME, lv_color_hex(0x00FF55), LV_OPA_COVER);
    lv_3dmesh_set_material(g_empty_wire, &ew);
    lv_3dmesh_set_opa(g_empty_wire, LV_OPA_TRANSP);

    g_anim_start = lv_tick_get();

    g_vp = lv_3dviewport_create(scr);
    lv_obj_set_size(g_vp, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(g_vp, LV_OPA_0, 0);
    lv_3dviewport_set_camera(g_vp, cam);
    lv_3dviewport_set_scene(g_vp, g_scene);
    lv_3dviewport_set_pickable(g_vp, false);

    g_anim_timer = lv_timer_create(s7_anim_timer_cb, 16, NULL);
    s7_anim_timer_cb(NULL);

    lv_obj_invalidate(g_vp);
    printf("LVGL_SCENARIO7: 20s keyframe side-view (grid + hero 1/3/4 focus), no video\n");
}

uint32_t lvgl_scenario7_get_anim_ms(void)
{
    return lv_tick_elaps(g_anim_start);
}

#else

void lvgl_scenario7_video_crop_create(void)
{
    lv_obj_t * scr = lv_screen_active();
    lv_obj_t * lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "LVGL_SCENARIO7: requires LV_USE_3D + LV_USE_DRAW_GPU_RENDERER");
    lv_obj_center(lbl);
    printf("LVGL_SCENARIO7: requires 3D + gpu_renderer\n");
}

uint32_t lvgl_scenario7_get_anim_ms(void)
{
    return 0;
}

#endif
