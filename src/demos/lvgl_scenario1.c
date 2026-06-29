/**
 * @file lvgl_scenario1.c — 场景一：9 宫格（从下到上、从近到远）+ 左右侧翼预览 + 触控全屏动效
 */

#include "lvgl/lvgl.h"
#include "lvgl_demos.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS && LV_USE_3DSTACK

#include <math.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#define LVGL_S1_TILE_COUNT 15
#define LVGL_S1_ANIM_MS      420
#define LVGL_S1_GRID_TILES   9
#define LVGL_S1_TILE_W       400
#define LVGL_S1_TILE_H       250
/** Grid center-to-center spacing (~20 unit edge gap). */
#define LVGL_S1_X_GAP        420.0f
#define LVGL_S1_Y_GAP        270.0f
#define LVGL_S1_PEEK_W       110.0f
#define LVGL_S1_PEEK_GAP_PX  20
#define LVGL_S1_FOCUS_Z      (-220.0f)
#define LVGL_S1_CAM_EYE_Z    900.0f
#define LVGL_S1_CAM_FOV_DEG  42.0f
/** 3D 虚拟背景：蓝天白云（AR 屏幕四角仍透明；3D 视口内填天空） */
#define LVGL_S1_SKY_Z        (-1280.0f)
#define LVGL_S1_SKY_W        4600.0f
#define LVGL_S1_SKY_H        2700.0f
#define LVGL_S1_SKY_TEX_W    1920
#define LVGL_S1_SKY_TEX_H    1080

typedef struct {
    lv_obj_t * mesh;
    float tile_w;
    float tile_h;
    float home_pos[3];
    float home_rot_y;  /* degrees */
    float home_scale[3];
    lv_color_t color;
} lvgl_s1_tile_t;

static lvgl_s1_tile_t g_tiles[LVGL_S1_TILE_COUNT];
static lv_obj_t * g_vp;
static lv_obj_t * g_scene;
static lv_obj_t * g_stack;
static int g_focused = -1;
static bool g_animating;
static lv_timer_t * g_anim_timer;

static int s_anim_target = -1;
static uint32_t s_anim_start;
static float s_from_pos[LVGL_S1_TILE_COUNT][3];
static float s_from_scale[LVGL_S1_TILE_COUNT][3];
static float s_from_rot_y[LVGL_S1_TILE_COUNT];
static lv_opa_t s_from_opa[LVGL_S1_TILE_COUNT];
static float s_focus_scale;

static const uint32_t tile_colors[LVGL_S1_TILE_COUNT] = {
    0xE53935, 0xFB8C00, 0xFDD835, 0x43A047, 0x1E88E5,
    0x8E24AA, 0x00ACC1, 0x6D4C41, 0x546E7A,
    0x26A69A, 0xEC407A,
    0xAB47BC, 0x26C6DA,
    0x5C6BC0, 0xFF7043,
};

static float ease_out_cubic(float t)
{
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

static void tile_apply(lvgl_s1_tile_t * t, float x, float y, float z,
                       float yaw_deg, float sx, float sy, float sz, lv_opa_t opa)
{
    lv_3dmesh_set_position(t->mesh, x, y, z);
    lv_3dmesh_set_rotation_y(t->mesh, yaw_deg);
    lv_3dmesh_set_scale(t->mesh, sx, sy, sz);
    lv_3dmesh_set_opa(t->mesh, opa);
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

static void restore_grid_tile_parent(int idx)
{
    if(idx < 0 || idx >= LVGL_S1_GRID_TILES) return;

    lv_obj_t * mesh = g_tiles[idx].mesh;
    if(!mesh || !g_stack || !g_scene) return;

    if(lv_obj_get_parent(mesh) == g_scene) {
        lv_obj_set_parent(mesh, g_stack);
    }
}

static void restore_all_grid_parents(void)
{
    for(int i = 0; i < LVGL_S1_GRID_TILES; i++) {
        restore_grid_tile_parent(i);
    }
}

/* GLES2 3D pass 无 depth test，绘制顺序 = 场景树子节点顺序；动效中置顶被点 tile */
static void bring_tile_to_draw_front(int idx)
{
    if(idx < 0 || idx >= LVGL_S1_TILE_COUNT) return;

    lv_obj_t * mesh = g_tiles[idx].mesh;
    if(!mesh || !g_scene) return;

    if(lv_obj_get_parent(mesh) != g_scene) {
        lv_obj_set_parent(mesh, g_scene);
    }
    lv_obj_move_foreground(mesh);
}

/** Scale so tile covers the viewport at LVGL_S1_FOCUS_Z (camera-matched). */
static float focus_scale_for_tile(float tile_w, float tile_h)
{
    if(tile_w < 1.0f) tile_w = (float)LVGL_S1_TILE_W;
    if(tile_h < 1.0f) tile_h = (float)LVGL_S1_TILE_H;

    lv_display_t * disp = lv_display_get_default();
    int32_t vp_w = disp ? lv_display_get_horizontal_resolution(disp) : 1920;
    int32_t vp_h = disp ? lv_display_get_vertical_resolution(disp) : 1080;
    float aspect = vp_h > 0 ? (float)vp_w / (float)vp_h : 16.0f / 9.0f;
    float depth = LVGL_S1_CAM_EYE_Z - LVGL_S1_FOCUS_Z;
    if(depth < 1.0f) depth = 1.0f;
    float half_fov_rad = (LVGL_S1_CAM_FOV_DEG * 0.5f) * (float)M_PI / 180.0f;
    float visible_h = 2.0f * depth * tanf(half_fov_rad);
    float visible_w = visible_h * aspect;
    float sx = visible_w / tile_w;
    float sy = visible_h / tile_h;
    return fmaxf(sx, sy);
}

static void apply_focused_steady_state(int focused_idx)
{
    float fs = focus_scale_for_tile(g_tiles[focused_idx].tile_w, g_tiles[focused_idx].tile_h);
    s_focus_scale = fs;

    for(int i = 0; i < LVGL_S1_TILE_COUNT; i++) {
        if(i == focused_idx) {
            tile_apply(&g_tiles[i], 0.0f, 0.0f, LVGL_S1_FOCUS_Z, 0.0f, fs, fs, fs, LV_OPA_COVER);
        }
        else {
            lvgl_s1_tile_t * t = &g_tiles[i];
            tile_apply(t,
                       t->home_pos[0], t->home_pos[1], t->home_pos[2] - 520.0f,
                       t->home_rot_y, 0.0f, 0.0f, 0.0f, LV_OPA_TRANSP);
        }
    }
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
        if(s_anim_target >= 0) {
            apply_focused_steady_state(s_anim_target);
        }
        else {
            restore_all_grid_parents();
            for(int i = 0; i < LVGL_S1_TILE_COUNT; i++) tile_apply_home(i, LV_OPA_COVER);
        }
        if(g_vp) lv_obj_invalidate(g_vp);
        return;
    }

    float e = ease_out_cubic(u);

    for(int i = 0; i < LVGL_S1_TILE_COUNT; i++) {
        lvgl_s1_tile_t * t = &g_tiles[i];
        float x, y, z, sx, sy, sz, yaw;
        lv_opa_t opa;

        if(s_anim_target >= 0 && i == s_anim_target) {
            x = s_from_pos[i][0] + (0.0f - s_from_pos[i][0]) * e;
            y = s_from_pos[i][1] + (0.0f - s_from_pos[i][1]) * e;
            z = s_from_pos[i][2] + (LVGL_S1_FOCUS_Z - s_from_pos[i][2]) * e;
            sx = s_from_scale[i][0] + (s_focus_scale - s_from_scale[i][0]) * e;
            sy = s_from_scale[i][1] + (s_focus_scale - s_from_scale[i][1]) * e;
            sz = s_from_scale[i][2] + (s_focus_scale - s_from_scale[i][2]) * e;
            yaw = s_from_rot_y[i] + (0.0f - s_from_rot_y[i]) * e;
            opa = (lv_opa_t)(s_from_opa[i] + ((int32_t)LV_OPA_COVER - s_from_opa[i]) * e);
        }
        else if(s_anim_target >= 0) {
            float shrink = 1.0f - e;
            x = s_from_pos[i][0];
            y = s_from_pos[i][1];
            z = s_from_pos[i][2] + (-520.0f) * e;
            sx = s_from_scale[i][0] * shrink;
            sy = s_from_scale[i][1] * shrink;
            sz = s_from_scale[i][2] * shrink;
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
            s_from_pos[i][2] = LVGL_S1_FOCUS_Z;
            s_from_scale[i][0] = s_from_scale[i][1] = s_from_scale[i][2] = s_focus_scale;
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
    if(target_idx >= 0) {
        s_focus_scale = focus_scale_for_tile(g_tiles[target_idx].tile_w, g_tiles[target_idx].tile_h);
    }
    snapshot_from_current();

    if(target_idx >= 0) {
        bring_tile_to_draw_front(target_idx);
    }
    else if(g_focused >= 0) {
        bring_tile_to_draw_front(g_focused);
    }

    g_animating = true;
    if(g_anim_timer) lv_timer_delete(g_anim_timer);
    g_anim_timer = lv_timer_create(anim_timer_cb, 16, NULL);
}

static int pick_tile_index(int32_t x, int32_t y)
{
    lv_obj_t * hit = lv_3dviewport_pick_at(g_vp, x, y);
    if(!hit) return -1;

    for(int i = 0; i < LVGL_S1_TILE_COUNT; i++) {
        if(g_tiles[i].mesh == hit) return i;
    }
    return -1;
}

#if LV_USE_SNAPSHOT

#if LV_FONT_SOURCE_HAN_SANS_SC_14_CJK
LV_FONT_DECLARE(lv_font_source_han_sans_sc_14_cjk)
#define LVGL_S1_DASH_TEXT_FONT (&lv_font_source_han_sans_sc_14_cjk)
#else
#define LVGL_S1_DASH_TEXT_FONT LV_FONT_DEFAULT
#endif

static void dashboard_clear_panel(lv_obj_t * obj)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(obj, LV_OPA_0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
}

static void dashboard_style_text(lv_obj_t * lbl, lv_color_t color, const lv_font_t * font)
{
    lv_obj_set_style_text_color(lbl, color, 0);
    if(font) lv_obj_set_style_text_font(lbl, font, 0);
}

static void dashboard_style_text_scaled(lv_obj_t * lbl, lv_color_t color, const lv_font_t * font, int32_t scale)
{
    dashboard_style_text(lbl, color, font);
    if(scale != 256) {
        lv_obj_set_style_transform_pivot_x(lbl, 0, 0);
        lv_obj_set_style_transform_pivot_y(lbl, 0, 0);
        lv_obj_set_style_transform_scale(lbl, scale, 0);
    }
}

#define LVGL_S1_NEWS_TEXT_SCALE 512
#define LVGL_S1_NEWS_TEXT_SIZE_PCT 70

static int32_t dashboard_scaled_line_h(const lv_font_t * font, int32_t scale)
{
    return (lv_font_get_line_height(font) * scale + 255) / 256;
}

static lv_obj_t * dashboard_add_news_line(lv_obj_t * parent, const char * text, lv_color_t color, int32_t scale)
{
    const lv_font_t * font = LVGL_S1_DASH_TEXT_FONT;
    const int32_t line_h = dashboard_scaled_line_h(font, scale);

    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, line_h);
    dashboard_clear_panel(row);
    lv_obj_add_flag(row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t * lbl = lv_label_create(row);
    lv_label_set_text(lbl, text);
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    dashboard_style_text_scaled(lbl, color, font, scale);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
    return row;
}

static void dashboard_add_news_block(lv_obj_t * parent, const char * line1, const char * line2,
                                     int32_t block_h, int32_t scale)
{
    lv_obj_t * blk = lv_obj_create(parent);
    lv_obj_set_width(blk, LV_PCT(100));
    lv_obj_set_height(blk, block_h);
    dashboard_clear_panel(blk);
    lv_obj_add_flag(blk, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_flex_flow(blk, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(blk, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(blk, 0, 0);

    dashboard_add_news_line(blk, line1, lv_color_hex(0x90A4AE), scale);
    if(line2 && line2[0] != '\0') {
        dashboard_add_news_line(blk, line2, lv_color_hex(0xECEFF1), scale);
    }
}

static void dashboard_populate(lv_obj_t * cont, int32_t w, int32_t h)
{
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(cont, 4, 0);

    const int32_t left_w = (w * 1) / 3;

    lv_obj_t * left = lv_obj_create(cont);
    lv_obj_set_size(left, left_w, LV_PCT(100));
    dashboard_clear_panel(left);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(left, 0, 0);

    lv_obj_t * top = lv_obj_create(left);
    lv_obj_set_width(top, LV_PCT(100));
    lv_obj_set_flex_grow(top, 1);
    dashboard_clear_panel(top);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(top, 2, 0);

    lv_obj_t * date_lbl = lv_label_create(top);
    lv_label_set_text(date_lbl, "周日 06/28");
    dashboard_style_text(date_lbl, lv_color_hex(0xCFD8DC), LVGL_S1_DASH_TEXT_FONT);

    lv_obj_t * batt = lv_label_create(top);
    lv_label_set_text(batt, LV_SYMBOL_BATTERY_FULL);
    dashboard_style_text(batt, lv_color_hex(0x69F0AE), LV_FONT_DEFAULT);

    lv_obj_t * mid = lv_obj_create(left);
    lv_obj_set_width(mid, LV_PCT(100));
    lv_obj_set_flex_grow(mid, 8);
    dashboard_clear_panel(mid);
    lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * time_lbl = lv_label_create(mid);
    lv_label_set_text(time_lbl, "14:32");
    dashboard_style_text(time_lbl, lv_color_hex(0xFFFFFF), LV_FONT_DEFAULT);
    lv_obj_set_style_transform_pivot_x(time_lbl, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(time_lbl, LV_PCT(50), 0);
    lv_obj_set_style_transform_scale(time_lbl, 925, 0);

    lv_obj_t * bot = lv_obj_create(left);
    lv_obj_set_width(bot, LV_PCT(100));
    lv_obj_set_flex_grow(bot, 1);
    dashboard_clear_panel(bot);
    lv_obj_set_flex_flow(bot, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bot, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_hor(bot, 2, 0);

    lv_obj_t * temp_row = lv_obj_create(bot);
    dashboard_clear_panel(temp_row);
    lv_obj_set_size(temp_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(temp_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(temp_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(temp_row, 2, 0);

    lv_obj_t * temp_lbl = lv_label_create(temp_row);
    lv_label_set_text(temp_lbl, "26");
    dashboard_style_text(temp_lbl, lv_color_hex(0xECEFF1), LV_FONT_DEFAULT);

    lv_obj_t * deg_lbl = lv_label_create(temp_row);
    lv_label_set_text(deg_lbl, "\xC2\xB0""C");
    dashboard_style_text(deg_lbl, lv_color_hex(0x90A4AE), LV_FONT_DEFAULT);

    lv_obj_t * bell = lv_label_create(bot);
    lv_label_set_text(bell, LV_SYMBOL_BELL);
    dashboard_style_text(bell, lv_color_hex(0xFFD54F), LV_FONT_DEFAULT);

    lv_obj_t * right = lv_obj_create(cont);
    lv_obj_set_size(right, w - left_w - 4, LV_PCT(100));
    lv_obj_set_flex_grow(right, 1);
    lv_obj_remove_flag(right, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(right, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_bg_opa(right, LV_OPA_0, 0);
    lv_obj_set_style_border_color(right, lv_color_hex(0x78909C), 0);
    lv_obj_set_style_border_width(right, 1, 0);
    lv_obj_set_style_radius(right, 4, 0);
    lv_obj_set_style_pad_all(right, 4, 0);
    lv_obj_set_style_pad_row(right, 0, 0);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    const int32_t inner_h = h - 8; /* cont vertical pad */
    const int32_t right_inner = inner_h - 8; /* right panel vertical pad */
    const int32_t lh_base = lv_font_get_line_height(LVGL_S1_DASH_TEXT_FONT);
    const int32_t line_h = right_inner / 5;
    int32_t news_scale = LVGL_S1_NEWS_TEXT_SCALE;
    if(lh_base > 0 && line_h > 0) {
        news_scale = (line_h * 256) / lh_base;
        if(news_scale < LVGL_S1_NEWS_TEXT_SCALE) news_scale = LVGL_S1_NEWS_TEXT_SCALE;
        if(news_scale > 896) news_scale = 896;
        news_scale = (news_scale * LVGL_S1_NEWS_TEXT_SIZE_PCT) / 100;
    }

    dashboard_add_news_block(right, "ETDay 新闻云     06/27", "快讯/五县市大雨特报", line_h * 2, news_scale);
    dashboard_add_news_block(right, "CNA 中央通讯社   06/27", "阿联飞弹警报误发出", line_h * 2, news_scale);
    dashboard_add_news_block(right, "ETDay 新闻云   06/27", NULL, line_h, news_scale);
}

#if LV_USE_APPWINDOW
static lv_obj_t * create_dashboard_thumb(lv_obj_t * parent, int32_t w, int32_t h)
{
    lv_obj_t * app = lv_appwindow_create(parent);
    lv_obj_set_size(app, w, h);
    lv_obj_t * cont = lv_appwindow_get_content(app);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cont, 10, 0);
    lv_obj_set_style_border_color(cont, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(cont, 2, 0);
    lv_obj_set_style_border_opa(cont, LV_OPA_40, 0);
    lv_obj_set_style_pad_all(cont, 4, 0);

    dashboard_populate(cont, w, h);
    lv_appwindow_capture_thumbnail(app);
    return app;
}

static lv_obj_t * create_thumb_source(lv_obj_t * parent, const char * title, uint32_t color_hex, int32_t w, int32_t h)
{
    lv_obj_t * app = lv_appwindow_create(parent);
    lv_obj_set_size(app, w, h);
    lv_obj_t * cont = lv_appwindow_get_content(app);
    lv_obj_set_style_bg_color(cont, lv_color_hex(color_hex), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cont, 10, 0);
    lv_obj_set_style_border_color(cont, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(cont, 2, 0);
    lv_obj_set_style_border_opa(cont, LV_OPA_60, 0);
    lv_obj_set_style_pad_all(cont, 6, 0);

    lv_obj_t * lbl = lv_label_create(cont);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl);

    lv_appwindow_capture_thumbnail(app);
    return app;
}
#else
static lv_obj_t * create_dashboard_thumb(lv_obj_t * parent, int32_t w, int32_t h)
{
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_size(cont, w, h);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cont, 10, 0);
    lv_obj_set_style_border_color(cont, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(cont, 2, 0);
    lv_obj_set_style_border_opa(cont, LV_OPA_40, 0);
    lv_obj_set_style_pad_all(cont, 4, 0);
    dashboard_populate(cont, w, h);
    return cont;
}

static lv_obj_t * create_thumb_source(lv_obj_t * parent, const char * title, uint32_t color_hex, int32_t w, int32_t h)
{
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_size(cont, w, h);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(cont, lv_color_hex(color_hex), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cont, 10, 0);
    lv_obj_set_style_border_color(cont, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(cont, 2, 0);
    lv_obj_set_style_border_opa(cont, LV_OPA_60, 0);
    lv_obj_set_style_pad_all(cont, 6, 0);

    lv_obj_t * lbl = lv_label_create(cont);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl);
    return cont;
}
#endif

static void mesh_apply_plane_thumb(lv_obj_t * mesh, lv_obj_t * thumb_src, float w, float h)
{
    lv_3d_snapshot_id_t snap;
#if LV_USE_APPWINDOW
    if(lv_obj_check_type(thumb_src, &lv_appwindow_class)) {
        snap = lv_appwindow_get_snapshot_id(thumb_src);
    }
    else
#endif
    {
        snap = lv_3d_plane_bake(thumb_src, LV_3D_PLANE_SRC_SNAPSHOT);
    }
    lv_3dmesh_set_box(mesh, w, h, 4);
    if(snap != LV_3D_SNAPSHOT_ID_NONE) {
        lv_3dmesh_set_plane_snapshot(mesh, snap);
    }
    else {
        lv_3d_material_t mat;
        lv_3d_material_init(&mat, LV_3D_MAT_OPAQUE, lv_color_hex(0x888888), LV_OPA_COVER);
        lv_3dmesh_set_material(mesh, &mat);
    }
}

static void add_cloud_puff(lv_obj_t * parent, int32_t x, int32_t y, int32_t w, int32_t h, lv_opa_t opa)
{
    lv_obj_t * p = lv_obj_create(parent);
    lv_obj_set_size(p, w, h);
    lv_obj_set_pos(p, x, y);
    lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(p, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(p, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(p, opa, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_set_style_shadow_width(p, 0, 0);
}

static void add_cloud(lv_obj_t * parent, int32_t cx, int32_t cy, int32_t scale, lv_opa_t opa)
{
    add_cloud_puff(parent, cx - 60 * scale / 100, cy, 120 * scale / 100, 52 * scale / 100, opa);
    add_cloud_puff(parent, cx + 10 * scale / 100, cy - 14 * scale / 100, 150 * scale / 100, 58 * scale / 100, opa);
    add_cloud_puff(parent, cx + 80 * scale / 100, cy + 8 * scale / 100, 110 * scale / 100, 48 * scale / 100, opa);
}

static lv_obj_t * create_sky_texture_source(lv_obj_t * parent)
{
    lv_obj_t * root = lv_obj_create(parent);
    lv_obj_set_size(root, LVGL_S1_SKY_TEX_W, LVGL_S1_SKY_TEX_H);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_radius(root, 0, 0);
    /* 上深下浅，接近真实天空 */
    lv_obj_set_style_bg_color(root, lv_color_hex(0x1565C0), 0);
    lv_obj_set_style_bg_grad_color(root, lv_color_hex(0x81D4FA), 0);
    lv_obj_set_style_bg_grad_dir(root, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    add_cloud(root, 140, 120, 115, LV_OPA_90);
    add_cloud(root, 480, 200, 125, LV_OPA_80);
    add_cloud(root, 860, 90, 110, LV_OPA_80);
    add_cloud(root, 1180, 180, 100, LV_OPA_80);
    add_cloud(root, 280, 380, 105, LV_OPA_70);
    add_cloud(root, 640, 320, 120, LV_OPA_80);
    add_cloud(root, 980, 420, 95, LV_OPA_70);
    add_cloud(root, 1520, 340, 90, LV_OPA_70);
    add_cloud(root, 1680, 120, 85, LV_OPA_60);
    return root;
}

static void setup_sky_backdrop(lv_obj_t * scene, lv_obj_t * bake_root)
{
    lv_obj_t * sky = lv_3dmesh_create(scene);
    lv_obj_t * src = create_sky_texture_source(bake_root);
    mesh_apply_plane_thumb(sky, src, LVGL_S1_SKY_W, LVGL_S1_SKY_H);
    lv_3dmesh_set_position(sky, 0.0f, 0.0f, LVGL_S1_SKY_Z);
    lv_obj_move_background(sky);
}

#else /* LV_USE_SNAPSHOT */

static void setup_sky_backdrop(lv_obj_t * scene, lv_obj_t * bake_root)
{
    LV_UNUSED(bake_root);
    lv_obj_t * sky = lv_3dmesh_create(scene);
    lv_3dmesh_set_box(sky, LVGL_S1_SKY_W, LVGL_S1_SKY_H, 4.0f);
    lv_3dmesh_set_wireframe(sky, false);
    lv_3d_material_t mat;
    lv_3d_material_init(&mat, LV_3D_MAT_OPAQUE, lv_color_hex(0x42A5F5), LV_OPA_COVER);
    lv_3dmesh_set_material(sky, &mat);
    lv_3dmesh_set_position(sky, 0.0f, 0.0f, LVGL_S1_SKY_Z);
    lv_obj_move_background(sky);
}

#endif /* LV_USE_SNAPSHOT */

static void vp_click_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(g_animating) return;

    lv_indev_t * indev = lv_indev_active();
    if(!indev) return;

    lv_point_t pt;
    lv_indev_get_point(indev, &pt);

    int idx = pick_tile_index(pt.x, pt.y);
    if(idx < 0) return;

    if(g_focused < 0) {
        start_anim(idx);
    }
    else if(idx == g_focused) {
        start_anim(-1);
    }
    else {
        restore_all_grid_parents();
        g_focused = -1;
        for(int i = 0; i < LVGL_S1_TILE_COUNT; i++) tile_apply_home(i, LV_OPA_COVER);
        start_anim(idx);
    }
}

/** Same scale curve as lv_3dstack_layout (row 0 = near / largest). */
static float grid_row_scale(uint32_t row)
{
    return 1.0f - (float)row * 0.08f;
}

/** Side peek X: grid outer edge + LVGL_S1_PEEK_GAP_PX screen pixels + half peek width. */
static float peek_x_for_row(float row_z, float peek_half_w)
{
    lv_display_t * disp = lv_display_get_default();
    int32_t vp_w = disp ? lv_display_get_horizontal_resolution(disp) : 1920;
    int32_t vp_h = disp ? lv_display_get_vertical_resolution(disp) : 1080;
    float aspect = vp_h > 0 ? (float)vp_w / (float)vp_h : 16.0f / 9.0f;
    float depth = LVGL_S1_CAM_EYE_Z - row_z;
    if(depth < 1.0f) depth = 1.0f;
    float half_fov_rad = (LVGL_S1_CAM_FOV_DEG * 0.5f) * (float)M_PI / 180.0f;
    float visible_w = 2.0f * depth * tanf(half_fov_rad) * aspect;
    float px_per_world = visible_w > 1.0f ? (float)vp_w / visible_w : 1.0f;
    float grid_outer = LVGL_S1_X_GAP + LVGL_S1_TILE_W * 0.5f;
    return grid_outer + ((float)LVGL_S1_PEEK_GAP_PX / px_per_world) + peek_half_w;
}

static void setup_peek_row_pair(lv_obj_t * scene, float row_y, float row_z, uint32_t row_idx,
                                int left_idx, int right_idx,
                                const char * title_l, const char * title_r,
                                uint32_t color_l, uint32_t color_r,
                                lv_obj_t * thumb_root)
{
    const float row_sc = grid_row_scale(row_idx);
    const float peek_w = LVGL_S1_PEEK_W;
    const float peek_h = (float)LVGL_S1_TILE_H;
    const float peek_world_half_w = peek_w * row_sc * 0.5f;
    const float peek_x = peek_x_for_row(row_z, peek_world_half_w);
    const float yaw_deg = 22.0f;

    lv_obj_t * peek_l = lv_3dmesh_create(scene);
#if LV_USE_SNAPSHOT
    {
        lv_obj_t * src = create_thumb_source(thumb_root, title_l, color_l, (int32_t)peek_w, (int32_t)peek_h);
        mesh_apply_plane_thumb(peek_l, src, peek_w, peek_h);
    }
#else
    lv_3dmesh_set_box(peek_l, peek_w, peek_h, 8);
    lv_3dmesh_set_wireframe(peek_l, false);
    {
        lv_3d_material_t mat;
        lv_3d_material_init(&mat, LV_3D_MAT_OPAQUE, lv_color_hex(color_l), LV_OPA_COVER);
        lv_3dmesh_set_material(peek_l, &mat);
    }
#endif
    lv_3dmesh_set_position(peek_l, -peek_x, row_y, row_z);
    lv_3dmesh_set_rotation_y(peek_l, yaw_deg);
    lv_3dmesh_set_scale(peek_l, row_sc, row_sc, row_sc);
    g_tiles[left_idx].mesh = peek_l;
    g_tiles[left_idx].tile_w = peek_w;
    g_tiles[left_idx].tile_h = peek_h;
    g_tiles[left_idx].color = lv_color_hex(color_l);
    g_tiles[left_idx].home_pos[0] = -peek_x;
    g_tiles[left_idx].home_pos[1] = row_y;
    g_tiles[left_idx].home_pos[2] = row_z;
    g_tiles[left_idx].home_rot_y = yaw_deg;
    g_tiles[left_idx].home_scale[0] = g_tiles[left_idx].home_scale[1] = g_tiles[left_idx].home_scale[2] = row_sc;

    lv_obj_t * peek_r = lv_3dmesh_create(scene);
#if LV_USE_SNAPSHOT
    {
        lv_obj_t * src = create_thumb_source(thumb_root, title_r, color_r, (int32_t)peek_w, (int32_t)peek_h);
        mesh_apply_plane_thumb(peek_r, src, peek_w, peek_h);
    }
#else
    lv_3dmesh_set_box(peek_r, peek_w, peek_h, 8);
    lv_3dmesh_set_wireframe(peek_r, false);
    {
        lv_3d_material_t mat;
        lv_3d_material_init(&mat, LV_3D_MAT_OPAQUE, lv_color_hex(color_r), LV_OPA_COVER);
        lv_3dmesh_set_material(peek_r, &mat);
    }
#endif
    lv_3dmesh_set_position(peek_r, peek_x, row_y, row_z);
    lv_3dmesh_set_rotation_y(peek_r, -yaw_deg);
    lv_3dmesh_set_scale(peek_r, row_sc, row_sc, row_sc);
    g_tiles[right_idx].mesh = peek_r;
    g_tiles[right_idx].tile_w = peek_w;
    g_tiles[right_idx].tile_h = peek_h;
    g_tiles[right_idx].color = lv_color_hex(color_r);
    g_tiles[right_idx].home_pos[0] = peek_x;
    g_tiles[right_idx].home_pos[1] = row_y;
    g_tiles[right_idx].home_pos[2] = row_z;
    g_tiles[right_idx].home_rot_y = -yaw_deg;
    g_tiles[right_idx].home_scale[0] = g_tiles[right_idx].home_scale[1] = g_tiles[right_idx].home_scale[2] = row_sc;
}

static void capture_grid_homes(lv_obj_t * stack)
{
    const float row_z[3] = { -350.0f, -650.0f, -950.0f };

    for(uint32_t i = 0; i < LVGL_S1_GRID_TILES; i++) {
        lv_obj_t * tile = lv_obj_get_child(stack, i);
        uint32_t r = i / 3;
        uint32_t c = i % 3;
        float cx = (float)c - 1.0f;
        float cy = 1.0f - (float)r;
        float sc = grid_row_scale(r);

        g_tiles[i].mesh = tile;
        g_tiles[i].tile_w = (float)LVGL_S1_TILE_W;
        g_tiles[i].tile_h = (float)LVGL_S1_TILE_H;
        g_tiles[i].color = lv_color_hex(tile_colors[i]);
        g_tiles[i].home_pos[0] = cx * LVGL_S1_X_GAP;
        g_tiles[i].home_pos[1] = cy * LVGL_S1_Y_GAP;
        g_tiles[i].home_pos[2] = row_z[r];
        g_tiles[i].home_scale[0] = g_tiles[i].home_scale[1] = g_tiles[i].home_scale[2] = sc;
        g_tiles[i].home_rot_y = 0.0f;
    }
}

void lvgl_scenario1_launcher_create(void)
{
    lv_display_t * disp = lv_display_get_default();
    if(disp) lv_display_set_antialiasing(disp, true);

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
    g_scene = scene;

    /* 缩略图 bake 树（隐藏）；天空贴图单独 off-screen bake，避免 hidden 父节点影响 snapshot */
    lv_obj_t * thumb_root = lv_obj_create(scr);
    lv_obj_add_flag(thumb_root, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t * sky_bake_root = lv_obj_create(scr);
    lv_obj_set_pos(sky_bake_root, -8000, 0);
    lv_obj_set_size(sky_bake_root, LVGL_S1_SKY_TEX_W, LVGL_S1_SKY_TEX_H);
    lv_obj_remove_flag(sky_bake_root, LV_OBJ_FLAG_SCROLLABLE);

    setup_sky_backdrop(scene, sky_bake_root);

    lv_obj_t * stack = lv_3dstack_create(scene);
    g_stack = stack;
    lv_3dstack_set_grid(stack, 3, 3);
    lv_3dstack_set_row_depth(stack, 0, -350);
    lv_3dstack_set_row_depth(stack, 1, -650);
    lv_3dstack_set_row_depth(stack, 2, -950);
    lv_3dstack_set_cell_spacing(stack, LVGL_S1_X_GAP, LVGL_S1_Y_GAP);

    static const char * app_titles[] = {
        "Maps", "Music", "Photos", "Mail", "Web",
        "Settings", "Notes", "Weather", "Files",
    };

    for(int i = 0; i < LVGL_S1_GRID_TILES; i++) {
        lv_obj_t * tile = lv_3dmesh_create(stack);
#if LV_USE_SNAPSHOT
        lv_obj_t * src;
        if(i == 4) {
            src = create_dashboard_thumb(thumb_root, LVGL_S1_TILE_W, LVGL_S1_TILE_H);
        }
        else {
            src = create_thumb_source(thumb_root, app_titles[i], tile_colors[i], LVGL_S1_TILE_W, LVGL_S1_TILE_H);
        }
        mesh_apply_plane_thumb(tile, src, LVGL_S1_TILE_W, LVGL_S1_TILE_H);
#else
        lv_3dmesh_set_box(tile, LVGL_S1_TILE_W, LVGL_S1_TILE_H, 8);
        lv_3dmesh_set_wireframe(tile, false);
        lv_3d_material_t mat;
        lv_3d_material_init(&mat, LV_3D_MAT_OPAQUE, lv_color_hex(tile_colors[i]), LV_OPA_COVER);
        lv_3dmesh_set_material(tile, &mat);
#endif
    }

    lv_3dstack_layout(stack);
    capture_grid_homes(stack);

    const float row_z[3] = { -350.0f, -650.0f, -950.0f };

    /* Rows 1–3: near row at screen bottom (world +Y), far row at top */
    setup_peek_row_pair(scene, LVGL_S1_Y_GAP, row_z[0], 0, 9, 10,
                        "App L1", "App R1", tile_colors[9], tile_colors[10], thumb_root);
    setup_peek_row_pair(scene, 0.0f, row_z[1], 1, 11, 12,
                        "App L2", "App R2", tile_colors[11], tile_colors[12], thumb_root);
    setup_peek_row_pair(scene, -LVGL_S1_Y_GAP, row_z[2], 2, 13, 14,
                        "App L3", "App R3", tile_colors[13], tile_colors[14], thumb_root);

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
