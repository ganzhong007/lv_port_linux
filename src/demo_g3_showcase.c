/*
 * Copyright (c) 2025 Even Technology
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file demo_g3_showcase.c
 * @brief 集中演示 G3 Glasses OS 的五种 LVGL 技巧。
 *
 * 布局（这个界面本身就是「布局」演示）：顶部标题栏，下面是 2x2 的玻璃卡片网格，
 * 每张卡片演示一种技巧：
 *
 *   +----------------------+----------------------+
 *   | PLAYER（均衡器+呼吸灯）| WORLD（渐变）         |
 *   +----------------------+----------------------+
 *   | HOVER（鼠标进入）      | BOOT（可重播）        |
 *   +----------------------+----------------------+
 *
 * 全部逻辑都自包含在本文件内，可独立编译展示，不依赖 demo_g3_os.c。
 */

#include "demo_g3_showcase.h"

#include <stdlib.h>

/* 全屏暗角/边条：1=开启，0=关闭（EVGPU 上径向 vignette 曾 cover 掉卡片）。 */
#ifndef G3_SHOWCASE_USE_VIGNETTE
#define G3_SHOWCASE_USE_VIGNETTE 0
#endif

/* Freeze player/boot animations for pixel-stable EVGPU vs C_R_T screenshots. */
#ifndef G3_SHOWCASE_NO_ANIM
#define G3_SHOWCASE_NO_ANIM 1
#endif

#if G3_SHOWCASE_NO_ANIM
static void dump_kick_cb(lv_timer_t * t)
{
    /* Keep dirtying the screen a few times so LVGL_GL_DUMP can fire after
     * animations are frozen (otherwise end_frame may only run once). */
    lv_obj_invalidate(lv_screen_active());
    int * n = lv_timer_get_user_data(t);
    if(n) {
        (*n)++;
        if(*n >= 12) {
            lv_timer_delete(t);
            lv_free(n);
        }
    }
}
#endif

/* ================= 外观配色 ================= */
#define G_GREEN     0x45FF8Au   /* 磷光绿 */
#define OP_BRIGHT   255
#define OP_DIM      120
#define OP_FAINT    60

#define SCREEN_W    g_screen_w
#define SCREEN_H    g_screen_h

static int32_t g_screen_w = 1200;
static int32_t g_screen_h = 720;

/* 本工程可用字体：montserrat 14/20/22/24/26/30/36/40。 */
#define FONT_SM     (&lv_font_montserrat_14)
#define FONT_MD     (&lv_font_montserrat_22)
#define FONT_LG     (&lv_font_montserrat_36)

/* ================= 共享状态 ================= */
static lv_style_transition_dsc_t g_hover_trans;
static lv_obj_t *                g_boot;        /* 当前的开机è¸º NULL */
static lv_obj_t *                g_boot_status; /* 开机层里的状态文字标签 */
static lv_timer_t *              g_boot_timer;
static int32_t                   g_boot_step;

static const char * const BOOT_STEPS[] = {
    "initializing display...",
    "calibrating field of view...",
    "loading G3 OS...",
    "ready",
};

/* ================= 小工具函数 ================= */

/* 一个无边框、无内边距、不可滚动的普通容器。 */
static lv_obj_t * plain(lv_obj_t * parent)
{
    lv_obj_t * o = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t * label(lv_obj_t * parent, const char * txt, const lv_font_t * font, lv_opa_t opa)
{
    lv_obj_t * l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_text_opa(l, opa, 0);
    return l;
}

/* ================= 技巧 3：鼠标悬停 ================= */

/* 指针进入/离开时切换 LV_STATE_HOVERED 状态（LVGL 没有 :hover 伪类）。 */
static void hover_event_cb(lv_event_t * e)
{
    lv_obj_t * o = lv_event_get_target_obj(e);

    if (lv_event_get_code(e) == LV_EVENT_HOVER_OVER) {
        lv_obj_add_state(o, LV_STATE_HOVERED);
    }
  else {
        lv_obj_remove_state(o, LV_STATE_HOVERED);
    }
}

/* 悬停时：更亮的填充 + 实心绿边 + 磷光辉光 + 轻微上浮，
 * 由 g_hover_trans 平滑过渡（对应 CSS 的 `.tile:hover` + transition）。 */
static void add_hover_glow(lv_obj_t * o)
{
    lv_obj_set_style_bg_color(o, lv_color_hex(G_GREEN), LV_STATE_HOVERED);
    lv_obj_set_style_bg_opa(o, 40, LV_STATE_HOVERED);
    lv_obj_set_style_border_color(o, lv_color_hex(G_GREEN), LV_STATE_HOVERED);
    lv_obj_set_style_border_opa(o, LV_OPA_COVER, LV_STATE_HOVERED);
    lv_obj_set_style_border_width(o, 2, LV_STATE_HOVERED);
    lv_obj_set_style_shadow_color(o, lv_color_hex(G_GREEN), LV_STATE_HOVERED);
    lv_obj_set_style_shadow_width(o, 24, LV_STATE_HOVERED);
    lv_obj_set_style_shadow_opa(o, 150, LV_STATE_HOVERED);
    lv_obj_set_style_shadow_spread(o, 1, LV_STATE_HOVERED);
    lv_obj_set_style_translate_y(o, -6, LV_STATE_HOVERED);
    lv_obj_set_style_transition(o, &g_hover_trans, 0);
    lv_obj_add_event_cb(o, hover_event_cb, LV_EVENT_HOVER_OVER, NULL);
    lv_obj_add_event_cb(o, hover_event_cb, LV_EVENT_HOVER_LEAVE, NULL);
}

/* 玻璃卡片：圆角矩形、淡绿填充、绿色边框、纵向 flex 布局。
 * 同时挂上悬停高亮，让每张卡片都能响应鼠标。 */
static lv_obj_t * make_card(lv_obj_t * parent)
{
    lv_obj_t * c = lv_obj_create(parent);
    lv_obj_set_style_bg_color(c, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_bg_opa(c, 16, 0);
    lv_obj_set_style_border_color(c, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_border_opa(c, 130, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, 16, 0);
    lv_obj_set_style_pad_all(c, 22, 0);
    lv_obj_set_style_pad_row(c, 12, 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    add_hover_glow(c);
    return c;
}

/* ================= 动画回调 ================= */

/* 技巧 1a：均衡器竖条——动画改变像素高度。 */
static void anim_bar_h_cb(void * obj, int32_t v)
{
    lv_obj_set_height((lv_obj_t *)obj, v);
}

/* 技巧 1b：呼吸「live」圆点——只改透明度（不缩放，避免圆点经方形变换图层闪烁）。 */
static void anim_breath_cb(void * obj, int32_t v)
{
    lv_obj_set_style_bg_opa((lv_obj_t *)obj, (lv_opa_t)(70 + (255 - 70) * v / 256), 0);
}

/* 技巧 4a：开机进度条——宽度 0..100 百分比。 */
static void anim_width_pct_cb(void * obj, int32_t v)
{
    lv_obj_set_width((lv_obj_t *)obj, v);
}

/* 技巧 4b：开机淡出——整个覆盖层的透明度。 */
static void anim_opa_cb(void * obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)(v > 255 ? 255 : v), 0);
}

static void anim_del_ready_cb(lv_anim_t * a)
{
    lv_obj_delete((lv_obj_t *)a->var);
}

/* ================= 技巧 2：world 背景 + 暗角 ================= */

/* 一层环境光斑：一个铺满全屏、以 (cx,cy) 为中心、由淡色向透明衰减的径向渐变对象，
 * 用原型 HTML 的 #world::before）。
 * cx/cy/radius 全用绝对像素，与 HTML 的 circle <radius>px at <cx> <cy> 一一对应。 */
static void ambient_glow(lv_obj_t * root, int32_t cx, int32_t cy, int32_t radius, uint32_t color, lv_opa_t opa)
{
    lv_grad_dsc_t * g = lv_malloc(sizeof(lv_grad_dsc_t));
    lv_memzero(g, sizeof(*g));
    /* 圆形径向：半径 = 圆心(cx,cy) 到 (cx, cy+radius) 的距离 = radius 像素 */
    lv_grad_radial_init(g, cx, cy, cx, cy + radius, LV_GRAD_EXTEND_PAD);
    g->stops_count    = 2;
    g->stops[0].color = lv_color_hex(color); g->stops[0].opa = opa; g->stops[0].frac = 0;
    g->stops[1].color = lv_color_hex(color); g->stops[1].opa = 0;   g->stops[1].frac = 255;

    int32_t hor = lv_display_get_horizontal_resolution(lv_display_get_default());
    int32_t ver = lv_display_get_vertical_resolution(lv_display_get_default());
    if(hor <= 0) hor = g_screen_w;
    if(ver <= 0) ver = g_screen_h;

    lv_obj_t * o = lv_obj_create(root);
    lv_obj_set_size(o, hor, ver);
    lv_obj_align(o, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);   /* 透明由每个 stop 的 opa 控制 */
    lv_obj_set_style_bg_grad(o, g, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
}

/* 压暗的「真实世界」：近黑的径向底色（对应原型 HTML 的 #world 色号），
 * 再叠加暖光 + 冷光两层环境光斑，还原原型那种蓝灰辉光感。 */
static void build_world(lv_obj_t * root)
{
    int32_t hor = lv_display_get_horizontal_resolution(lv_display_get_default());
    int32_t ver = lv_display_get_vertical_resolution(lv_display_get_default());
    if(hor <= 0) hor = 800;
    if(ver <= 0) ver = 480;

    static lv_grad_dsc_t grad;
    lv_obj_t * world = lv_obj_create(root);
    lv_memzero(&grad, sizeof(grad));
    /* Bug fix (2026-07-19): use % of display so the radial gradient
     * is centered correctly on 800x480 (Orangepi) as well as 1200x720. */
    int32_t cx1 = hor / 2;
    int32_t cy1 = ver * 8 / 100;
    int32_t r1  = ver * 118 / 100;     /* ~ 567px on 480h, ~ 850px on 720h */
    lv_grad_radial_init(&grad, cx1, cy1, cx1, cy1 + r1, LV_GRAD_EXTEND_PAD);
    grad.stops_count    = 4;
    grad.stops[0].color = lv_color_hex(0x11161c); grad.stops[0].opa = LV_OPA_COVER; grad.stops[0].frac = 0;
    grad.stops[1].color = lv_color_hex(0x0a0d11); grad.stops[1].opa = LV_OPA_COVER; grad.stops[1].frac = 97;
    grad.stops[2].color = lv_color_hex(0x05070a); grad.stops[2].opa = LV_OPA_COVER; grad.stops[2].frac = 179;
    grad.stops[3].color = lv_color_hex(0x020304); grad.stops[3].opa = LV_OPA_COVER; grad.stops[3].frac = 255;

    lv_obj_set_size(world, hor, ver);
    lv_obj_align(world, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_border_width(world, 0, 0);
    lv_obj_set_style_radius(world, 0, 0);
    lv_obj_set_style_pad_all(world, 0, 0);
    lv_obj_set_style_bg_opa(world, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad(world, &grad, 0);
    lv_obj_remove_flag(world, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(world, LV_OBJ_FLAG_CLICKABLE);

    /* 暖光(右上 72%, 30%) + 冷光(左下 30%, 96%)，半径 45% of vertical.
     * 864=72%*1200, 216=30%*720, 360=30%*1200, 691=96%*720。 */
    int32_t glow_r = ver * 45 / 100;
    ambient_glow(root, hor * 72 / 100, ver * 30 / 100, glow_r, 0x786e5a, 46);
    ambient_glow(root, hor * 30 / 100, ver * 96 / 100, glow_r, 0x283c50, 46);
}

/* 镜片暗角：四边半透明黑条（不用全屏径向渐变，避免 EVGPU cover / 单 FILL 问题）。 */
#if G3_SHOWCASE_USE_VIGNETTE
static void build_vignette(lv_obj_t * root)
{
    int32_t hor = lv_display_get_horizontal_resolution(lv_display_get_default());
    int32_t ver = lv_display_get_vertical_resolution(lv_display_get_default());
    if(hor <= 0) hor = 800;
    if(ver <= 0) ver = 480;

    int32_t band = LV_MAX(24, ver * 12 / 100);

    typedef struct { int32_t x, y, w, h; lv_opa_t opa; } band_t;
    const band_t bands[] = {
        {0, 0, hor, band, 180},                 /* top */
        {0, ver - band, hor, band, 200},         /* bottom */
        {0, 0, band, ver, 160},                  /* left */
        {hor - band, 0, band, ver, 160},         /* right */
    };

    for(unsigned i = 0; i < sizeof(bands) / sizeof(bands[0]); i++) {
        lv_obj_t * b = lv_obj_create(root);
        lv_obj_set_pos(b, bands[i].x, bands[i].y);
        lv_obj_set_size(b, bands[i].w, bands[i].h);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(b, bands[i].opa, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_radius(b, 0, 0);
        lv_obj_set_style_pad_all(b, 0, 0);
        lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
    }
}
#endif /* G3_SHOWCASE_USE_VIGNETTE */

/* 「world」卡片里带文字标签的小径向渐变色卡。 */
static void grad_swatch(lv_obj_t * parent, uint32_t c0, uint32_t c1, const char * name)
{
    lv_obj_t * wrap = plain(parent);
    lv_obj_set_size(wrap, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(wrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(wrap, 6, 0);

    lv_grad_dsc_t * grad = lv_malloc(sizeof(lv_grad_dsc_t));
    lv_memzero(grad, sizeof(*grad));
    lv_grad_radial_init(grad, LV_GRAD_CENTER, LV_GRAD_CENTER, 80, 80,
                        LV_GRAD_EXTEND_PAD);
    grad->stops_count    = 2;
    grad->stops[0].color = lv_color_hex(c0); grad->stops[0].opa = LV_OPA_COVER; grad->stops[0].frac = 0;
    grad->stops[1].color = lv_color_hex(c1); grad->stops[1].opa = LV_OPA_COVER; grad->stops[1].frac = 255;

    lv_obj_t * sw = lv_obj_create(wrap);
    lv_obj_set_size(sw, 92, 92);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(sw, 1, 0);
    lv_obj_set_style_border_color(sw, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_border_opa(sw, OP_FAINT, 0);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad(sw, grad, 0);
    lv_obj_remove_flag(sw, LV_OBJ_FLAG_SCROLLABLE);

    label(wrap, name, FONT_SM, OP_DIM);
}

/* ================= 技巧 1：PLAYER 卡片 ================= */

static void place_card(lv_obj_t * card, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    /* Compact padding on small panels (e.g. Orangepi 800x480). */
    if(h < 220) {
        lv_obj_set_style_pad_all(card, 10, 0);
        lv_obj_set_style_pad_row(card, 6, 0);
    }
}

static void build_card_player(lv_obj_t * parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t * card = make_card(parent);
    place_card(card, x, y, w, h);

    label(card, "PLAYER", FONT_MD, OP_BRIGHT);
    label(card, "bars + breathing dot", FONT_SM, OP_DIM);

    /* 一行：[ live 圆点 ]  正在播放 */
    lv_obj_t * live = plain(card);
    lv_obj_set_size(live, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(live, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(live, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(live, 10, 0);

    lv_obj_t * dot = lv_obj_create(live);
    lv_obj_set_size(dot, 14, 14);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    label(live, "LIVE", FONT_SM, OP_DIM);

#if G3_SHOWCASE_NO_ANIM
    /* Static mid-breath: full green dot. */
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
#else
    /* 呼吸灯：透明度 255<->70，1.7s 往返一圈，无限循环，ease-in-out。 */
    lv_anim_t ba;
    lv_anim_init(&ba);
    lv_anim_set_var(&ba, dot);
    lv_anim_set_exec_cb(&ba, anim_breath_cb);
    lv_anim_set_values(&ba, 256, 0);
    lv_anim_set_duration(&ba, 850);
    lv_anim_set_playback_duration(&ba, 850);
    lv_anim_set_repeat_count(&ba, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&ba, lv_anim_path_ease_in_out);
    lv_anim_start(&ba);
#endif

    /* 弹性空白，把均衡器顶到卡片底部。 */
    lv_obj_t * grow = plain(card);
    lv_obj_set_flex_grow(grow, 1);
    lv_obj_set_width(grow, lv_pct(100));

    /* 均衡器：5 根竖条；静态模式用固定高度，便于截图像素对比。 */
    static const int32_t eq_dur[5]   = { 900, 750, 1050, 850, 900 };
    static const int32_t eq_delay[5] = { 0, 180, 360, 100, 280 };
    static const int32_t eq_h_static[5] = { 28, 64, 84, 48, 36 };

    lv_obj_t * eq = plain(card);
    lv_obj_set_size(eq, LV_SIZE_CONTENT, 90);
    lv_obj_set_flex_flow(eq, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(eq, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(eq, 6, 0);

    for (int32_t i = 0; i < 5; i++) {
        lv_obj_t * bar = lv_obj_create(eq);
        lv_obj_set_width(bar, 9);
#if G3_SHOWCASE_NO_ANIM
        lv_obj_set_height(bar, eq_h_static[i]);
#else
        lv_obj_set_height(bar, 12);
#endif
        lv_obj_set_style_radius(bar, 3, 0);
        lv_obj_set_style_bg_color(bar, lv_color_hex(G_GREEN), 0);
        lv_obj_set_style_bg_opa(bar, OP_BRIGHT, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_shadow_color(bar, lv_color_hex(G_GREEN), 0);
        lv_obj_set_style_shadow_width(bar, 24, 0);
        lv_obj_set_style_shadow_opa(bar, 90, 0);
        lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

#if !G3_SHOWCASE_NO_ANIM
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, bar);
        lv_anim_set_exec_cb(&a, anim_bar_h_cb);
        lv_anim_set_values(&a, 12, 84);
        lv_anim_set_duration(&a, eq_dur[i] / 2);
        lv_anim_set_playback_duration(&a, eq_dur[i] / 2);
        lv_anim_set_delay(&a, eq_delay[i]);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_start(&a);
#else
        LV_UNUSED(eq_dur);
        LV_UNUSED(eq_delay);
#endif
    }
}

/* ================= 技巧 2：WORLD 卡片 ================= */

static void build_card_world(lv_obj_t * parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t * card = make_card(parent);
    place_card(card, x, y, w, h);

    label(card, "WORLD", FONT_MD, OP_BRIGHT);
    label(card, "radial gradient samples", FONT_SM, OP_DIM);

    lv_obj_t * grow = plain(card);
    lv_obj_set_flex_grow(grow, 1);
    lv_obj_set_width(grow, lv_pct(100));

    lv_obj_t * row = plain(card);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    grad_swatch(row, 0x243444, 0x05090e, "world");
    grad_swatch(row, 0x0d3b24, 0x020806, "glow");
    grad_swatch(row, 0x000000, 0x000000, "fade");
}

/* ================= 技巧 3：HOVER 卡片 ================= */

static void build_card_hover(lv_obj_t * parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t * card = make_card(parent);
    place_card(card, x, y, w, h);

    label(card, "HOVER", FONT_MD, OP_BRIGHT);
    label(card, "move the mouse over any tile", FONT_SM, OP_DIM);

    lv_obj_t * grow = plain(card);
    lv_obj_set_flex_grow(grow, 1);
    lv_obj_set_width(grow, lv_pct(100));

    lv_obj_t * row = plain(card);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int32_t i = 0; i < 3; i++) {
        lv_obj_t * mini = lv_obj_create(row);
        int32_t mini_sz = (h < 220) ? 56 : 96;
        lv_obj_set_size(mini, mini_sz, mini_sz);
        lv_obj_set_style_radius(mini, 14, 0);
        lv_obj_set_style_bg_color(mini, lv_color_hex(G_GREEN), 0);
        lv_obj_set_style_bg_opa(mini, 16, 0);
        lv_obj_set_style_border_color(mini, lv_color_hex(G_GREEN), 0);
        lv_obj_set_style_border_opa(mini, 130, 0);
        lv_obj_set_style_border_width(mini, 1, 0);
        lv_obj_remove_flag(mini, LV_OBJ_FLAG_SCROLLABLE);
        add_hover_glow(mini);

        lv_obj_t * l = label(mini, LV_SYMBOL_OK, FONT_MD, OP_BRIGHT);
        lv_obj_center(l);
    }
}

/* ================= 技巧 4：BOOT 卡片 ================= */

static void boot_timer_cb(lv_timer_t * t)
{
    g_boot_step++;
    if (g_boot_step < 4) {
        lv_label_set_text(g_boot_status, BOOT_STEPS[g_boot_step]);
        return;
    }
    lv_timer_delete(t);
    g_boot_timer = NULL;

    /* 把整个覆盖层淡出，然后删除它。 */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, g_boot);
    lv_anim_set_exec_cb(&a, anim_opa_cb);
    lv_anim_set_values(&a, 255, 0);
    lv_anim_set_duration(&a, 600);
    lv_anim_set_completed_cb(&a, anim_del_ready_cb);
    lv_anim_start(&a);
    g_boot = NULL;
}

/* 在顶层构建（或重建）开机覆盖层并启动它的动画。 */
static void run_boot(void)
{
    if (g_boot != NULL) {
        return;                 /* 正在播放，忽略 */
    }

    int32_t hor = lv_display_get_horizontal_resolution(lv_display_get_default());
    int32_t ver = lv_display_get_vertical_resolution(lv_display_get_default());
    if(hor <= 0) hor = 800;
    if(ver <= 0) ver = 480;

    lv_obj_t * boot = lv_obj_create(lv_layer_top());
    lv_obj_set_size(boot, hor, ver);
    lv_obj_align(boot, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(boot, lv_color_hex(0x000000), 0);
    /* Bug fix (2026-07-19): boot's BLACK OPAQUE bg was hiding the entire
     * screen layer's G3 content. Make bg fully transparent; the centered
     * mark/title/track widgets inside are visible on their own. */
    lv_obj_set_style_bg_opa(boot, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(boot, 0, 0);
    lv_obj_set_style_radius(boot, 0, 0);
    lv_obj_set_flex_flow(boot, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(boot, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(boot, 20, 0);
    lv_obj_remove_flag(boot, LV_OBJ_FLAG_SCROLLABLE);
    g_boot = boot;

    /* 双圆环标志。 */
    lv_obj_t * mark = plain(boot);
    lv_obj_set_size(mark, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(mark, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(mark, 10, 0);
    for (int32_t i = 0; i < 2; i++) {
        lv_obj_t * ring = lv_obj_create(mark);
        lv_obj_set_size(ring, 40, 40);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(ring, lv_color_hex(G_GREEN), 0);
        lv_obj_set_style_border_opa(ring, OP_BRIGHT, 0);
        lv_obj_set_style_border_width(ring, 3, 0);
        lv_obj_remove_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    }

    lv_obj_t * title = label(boot, "EVEN  G3", FONT_LG, OP_BRIGHT);
    lv_obj_set_style_text_letter_space(title, 6, 0);

    /* 进度条：底槽 + 填充条（动画改变的是填充条的宽度）。 */
    lv_obj_t * track = lv_obj_create(boot);
    lv_obj_set_size(track, 180, 4);
    lv_obj_set_style_radius(track, 2, 0);
    lv_obj_set_style_bg_color(track, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_bg_opa(track, 40, 0);
    lv_obj_set_style_border_width(track, 0, 0);
    lv_obj_set_style_pad_all(track, 0, 0);
    lv_obj_remove_flag(track, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * fill = lv_obj_create(track);
    lv_obj_set_height(fill, lv_pct(100));
    lv_obj_set_width(fill, 0);
    lv_obj_align(fill, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_radius(fill, 2, 0);
    lv_obj_set_style_bg_color(fill, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_bg_opa(fill, OP_BRIGHT, 0);
    lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_set_style_pad_all(fill, 0, 0);
    lv_obj_set_style_shadow_color(fill, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_shadow_width(fill, 14, 0);
    lv_obj_set_style_shadow_opa(fill, 160, 0);
    lv_obj_remove_flag(fill, LV_OBJ_FLAG_SCROLLABLE);

    g_boot_status = label(boot, BOOT_STEPS[0], FONT_SM, OP_DIM);

    /* 进度：0% -> 100%，1700ms，ease-out。 */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, fill);
    lv_anim_set_exec_cb(&a, anim_width_pct_cb);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_duration(&a, 1700);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    /* 状态文字每 480ms 换一句；第 4 步触发淡出。 */
    g_boot_step  = 0;
    g_boot_timer = lv_timer_create(boot_timer_cb, 480, NULL);
}

static void boot_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    run_boot();
}

static void build_card_boot(lv_obj_t * parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t * card = make_card(parent);
    place_card(card, x, y, w, h);

    label(card, "BOOT", FONT_MD, OP_BRIGHT);
    label(card, "progress + status text + fade out", FONT_SM, OP_DIM);

    lv_obj_t * grow = plain(card);
    lv_obj_set_flex_grow(grow, 1);
    lv_obj_set_width(grow, lv_pct(100));

    /* 一个可点击的「重播」胶囊按钮，复用悬停高亮。 */
    lv_obj_t * btn = lv_obj_create(card);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_bg_opa(btn, 24, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_border_opa(btn, 150, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_pad_hor(btn, 26, 0);
    lv_obj_set_style_pad_ver(btn, 14, 0);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    add_hover_glow(btn);
    lv_obj_add_event_cb(btn, boot_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * l = label(btn, LV_SYMBOL_REFRESH "  Replay boot", FONT_MD, OP_BRIGHT);
    lv_obj_center(l);
}

/* ================= 对外入口 ================= */

void demo_g3_showcase_init(void)
{
    /* 共享的悬停过渡（指定哪些属性做过渡）。 */
    static const lv_style_prop_t hover_props[] = {
        LV_STYLE_BORDER_OPA, LV_STYLE_BORDER_WIDTH, LV_STYLE_BG_OPA,
        LV_STYLE_SHADOW_WIDTH, LV_STYLE_SHADOW_OPA, LV_STYLE_TRANSLATE_Y,
        LV_STYLE_PROP_INV
    };
    lv_style_transition_dsc_init(&g_hover_trans, hover_props, lv_anim_path_ease_out, 220, 0, NULL);

    int32_t hor = lv_display_get_horizontal_resolution(lv_display_get_default());
    int32_t ver = lv_display_get_vertical_resolution(lv_display_get_default());
    if(hor <= 0) hor = 800;
    if(ver <= 0) ver = 480;
    g_screen_w = hor;
    g_screen_h = ver;

    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x02040a), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Step 1: only add world back (no vignette / no boot). */
    build_world(scr);

    int32_t pad_x = LV_MAX(8, hor * 5 / 100);
    int32_t pad_top = LV_MAX(6, ver * 4 / 100);
    int32_t pad_bot = LV_MAX(6, ver * 5 / 100);
    int32_t gap = LV_MAX(6, LV_MIN(hor, ver) * 3 / 100);
    int32_t header_h = (ver >= 600) ? 70 : 48;

    lv_obj_t * header = plain(scr);
    lv_obj_set_size(header, hor - 2 * pad_x, header_h);
    lv_obj_set_pos(header, pad_x, pad_top);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(header, 2, 0);
    lv_obj_t * h = label(header, "G3  UX  SHOWCASE",
                         (ver >= 600) ? FONT_LG : FONT_MD, OP_BRIGHT);
    lv_obj_set_style_text_letter_space(h, 4, 0);
    label(header, "player  -  world  -  hover  -  boot  -  layout", FONT_SM, OP_DIM);

    int32_t grid_y = pad_top + header_h + gap;
    int32_t grid_h = ver - grid_y - pad_bot;
    int32_t grid_w = hor - 2 * pad_x;
    int32_t cw = (grid_w - gap) / 2;
    int32_t ch = (grid_h - gap) / 2;
    if(cw < 80) cw = 80;
    if(ch < 80) ch = 80;

    build_card_player(scr, pad_x,               grid_y,               cw, ch);
    build_card_world (scr, pad_x + cw + gap,    grid_y,               cw, ch);
    build_card_hover (scr, pad_x,               grid_y + ch + gap,    cw, ch);
    build_card_boot  (scr, pad_x + cw + gap,    grid_y + ch + gap,    cw, ch);

#if G3_SHOWCASE_USE_VIGNETTE
    build_vignette(scr);
#endif

#if G3_SHOWCASE_NO_ANIM
    if(getenv("LVGL_GL_DUMP")) {
        int * n = lv_malloc(sizeof(int));
        if(n) {
            *n = 0;
            lv_timer_create(dump_kick_cb, 80, n);
        }
    }
#endif
}
