/**
 * @file lvgl_scenario8.c — GPU 2D edge-case acceptance (sw_raster=0).
 *
 * Exercises: Freetype vector outline label, DIFFERENCE + transform image,
 * ARGB8888 bitmap_mask, ARC + img_src, mixed VECTOR glyph iterate + atlas overflow.
 */

#include "lvgl/lvgl.h"
#include "lvgl_demos.h"

#if LV_USE_DRAW_GPU_RENDERER

#include "draw/gpu_renderer/lv_draw_gpu_renderer.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#if LV_USE_FREETYPE
#include "lvgl/font/lv_freetype.h"
#endif

LV_IMAGE_DECLARE(s8_argb_mask);
LV_IMAGE_DECLARE(s8_arc_img);
LV_IMAGE_DECLARE(s8_diff_img);

#define S8_PANEL_W   420
#define S8_PANEL_H   300
#define S8_BOTTOM_H  220

static lv_obj_t * g_overflow_label;
static char g_overflow_text[1600];
static lv_font_t * g_ft_font;

static const lv_font_t * s8_bitmap_font(void)
{
#if LV_FONT_MONTSERRAT_14
    return &lv_font_montserrat_14;
#elif LV_FONT_MONTSERRAT_48
    return &lv_font_montserrat_48;
#else
    return LV_FONT_DEFAULT;
#endif
}

static const char * s8_pick_font_path(void)
{
    static const char * const paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        NULL,
    };
    for(int i = 0; paths[i]; i++) {
        if(access(paths[i], R_OK) == 0) return paths[i];
    }
    return NULL;
}

static lv_obj_t * s8_panel(lv_obj_t * parent, lv_align_t align, int32_t x_ofs, int32_t y_ofs, uint32_t bg_hex)
{
    lv_obj_t * p = lv_obj_create(parent);
    lv_obj_set_size(p, S8_PANEL_W, S8_PANEL_H);
    lv_obj_align(p, align, x_ofs, y_ofs);
    lv_obj_set_style_bg_color(p, lv_color_hex(bg_hex), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p, 2, 0);
    lv_obj_set_style_border_color(p, lv_color_hex(0x455A64), 0);
    lv_obj_set_style_radius(p, 8, 0);
    lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

static void s8_utf8_append(char ** p, uint32_t cp)
{
    if(cp < 0x80) {
        *(*p)++ = (char)cp;
    }
    else if(cp < 0x800) {
        *(*p)++ = (char)(0xC0 | (cp >> 6));
        *(*p)++ = (char)(0x80 | (cp & 0x3F));
    }
    else {
        *(*p)++ = (char)(0xE0 | (cp >> 12));
        *(*p)++ = (char)(0x80 | ((cp >> 6) & 0x3F));
        *(*p)++ = (char)(0x80 | (cp & 0x3F));
    }
}

static void s8_build_overflow_text(void)
{
    char * p = g_overflow_text;
    const char * end = p + sizeof(g_overflow_text) - 8;
    for(uint32_t i = 0; i < 80 && p < end; i++) {
        s8_utf8_append(&p, 32u + i);
    }
    *p = '\0';
}

#if LV_USE_FREETYPE
static lv_font_t * s8_create_ft_font(void)
{
    const char * path = s8_pick_font_path();
    if(!path) {
        printf("LVGL_SCENARIO8: WARN no DejaVuSans.ttf — outline/vector cases skipped\n");
        return NULL;
    }
    lv_font_t * font = lv_freetype_font_create(path, LV_FREETYPE_FONT_RENDER_MODE_OUTLINE, 28,
                                               LV_FREETYPE_FONT_STYLE_NORMAL);
    if(!font) {
        printf("LVGL_SCENARIO8: WARN freetype font create failed: %s\n", path);
    }
    return font;
}
#endif

static void s8_outline_label(lv_obj_t * panel)
{
    lv_obj_t * title = lv_label_create(panel);
    lv_label_set_text(title, "1 Outline");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

#if LV_USE_FREETYPE
    if(g_ft_font) {
        lv_obj_t * lbl = lv_label_create(panel);
        lv_label_set_text(lbl, "GPU Vector Outline");
        lv_obj_set_style_text_font(lbl, g_ft_font, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xE3F2FD), 0);
        lv_obj_set_style_text_outline_stroke_width(lbl, 6, 0);
        lv_obj_set_style_text_outline_stroke_color(lbl, lv_color_hex(0xFF5722), 0);
        lv_obj_set_style_text_outline_stroke_opa(lbl, LV_OPA_COVER, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 10);
        return;
    }
#endif
    lv_obj_t * hint = lv_label_create(panel);
    lv_label_set_text(hint, "(FREETYPE disabled)");
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 10);
}

static void s8_diff_transform(lv_obj_t * panel)
{
    lv_obj_t * title = lv_label_create(panel);
    lv_label_set_text(title, "2 DIFFERENCE+rot");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t * bg = lv_obj_create(panel);
    lv_obj_set_size(bg, 180, 180);
    lv_obj_align(bg, LV_ALIGN_CENTER, 0, 16);
    lv_obj_set_style_bg_color(bg, lv_color_hex(0xE91E63), 0);
    lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bg, 0, 0);
    lv_obj_remove_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * img = lv_image_create(panel);
    lv_image_set_src(img, &s8_diff_img);
    lv_image_set_rotation(img, 450);
    lv_image_set_pivot(img, 12, 12);
    lv_obj_set_style_blend_mode(img, LV_BLEND_MODE_DIFFERENCE, 0);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 16);
}

static void s8_argb_mask_panel(lv_obj_t * panel)
{
    lv_obj_t * title = lv_label_create(panel);
    lv_label_set_text(title, "3 ARGB mask");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t * img = lv_image_create(panel);
    lv_image_set_src(img, &s8_arc_img);
    lv_image_set_bitmap_map_src(img, &s8_argb_mask);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 16);
}

static void s8_arc_image(lv_obj_t * panel)
{
    lv_obj_t * title = lv_label_create(panel);
    lv_label_set_text(title, "4 ARC+img");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t * arc = lv_arc_create(panel);
    lv_obj_remove_style_all(arc);
    lv_obj_set_size(arc, 200, 200);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_angles(arc, 40, 310);
    lv_obj_set_style_arc_width(arc, 24, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 24, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x4CAF50), LV_PART_INDICATOR);
    lv_obj_set_style_arc_image_src(arc, &s8_arc_img, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_align(arc, LV_ALIGN_CENTER, 0, 16);
    lv_arc_set_value(arc, 80);
}

static void s8_vector_iterate(lv_obj_t * parent)
{
    lv_obj_t * title = lv_label_create(parent);
    lv_label_set_text(title, "5 VECTOR iterate + atlas overflow");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

#if LV_USE_FREETYPE
    if(g_ft_font) {
        lv_obj_t * vec = lv_label_create(parent);
        lv_label_set_text(vec, "Iterate: AαβγΔΩ文é");
        lv_obj_set_style_text_font(vec, g_ft_font, 0);
        lv_obj_set_style_text_color(vec, lv_color_hex(0xFFEB3B), 0);
        lv_obj_align(vec, LV_ALIGN_LEFT_MID, 8, -20);
    }
#endif

    s8_build_overflow_text();
    g_overflow_label = lv_label_create(parent);
    lv_label_set_long_mode(g_overflow_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(g_overflow_label, lv_pct(95));
    lv_obj_set_style_text_font(g_overflow_label, s8_bitmap_font(), 0);
    lv_obj_set_style_text_color(g_overflow_label, lv_color_hex(0xB2DFDB), 0);
    lv_label_set_text(g_overflow_label, g_overflow_text);
    lv_obj_align(g_overflow_label, LV_ALIGN_BOTTOM_MID, 0, -8);
}

uint32_t lvgl_scenario8_get_glyph_overflow_count(void)
{
    return lv_gpu_renderer_gles2_glyph_overflow_count();
}

void lvgl_scenario8_gpu2d_edges_create(void)
{
    lv_gpu_renderer_set_ui_mode(LV_GPU_RENDERER_UI_GENERIC);

#if LV_USE_FREETYPE
    g_ft_font = s8_create_ft_font();
#endif

    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x121212), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * p1 = s8_panel(scr, LV_ALIGN_TOP_LEFT, 24, 24, 0x263238);
    lv_obj_t * p2 = s8_panel(scr, LV_ALIGN_TOP_RIGHT, -24, 24, 0x37474F);
    lv_obj_t * p3 = s8_panel(scr, LV_ALIGN_LEFT_MID, 24, 0, 0x263238);
    lv_obj_t * p4 = s8_panel(scr, LV_ALIGN_RIGHT_MID, -24, 0, 0x37474F);

    s8_outline_label(p1);
    s8_diff_transform(p2);
    s8_argb_mask_panel(p3);
    s8_arc_image(p4);

    lv_obj_t * bottom = lv_obj_create(scr);
    lv_obj_set_size(bottom, lv_pct(96), S8_BOTTOM_H);
    lv_obj_align(bottom, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_bg_color(bottom, lv_color_hex(0x1B5E20), 0);
    lv_obj_set_style_bg_opa(bottom, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bottom, 8, 0);
    lv_obj_remove_flag(bottom, LV_OBJ_FLAG_SCROLLABLE);
    s8_vector_iterate(bottom);

    lv_obj_invalidate(scr);
    printf("LVGL_SCENARIO8: GPU 2D edge demo ready (overflow_chars=%u)\n",
           (unsigned)strlen(g_overflow_text));
}

#else

uint32_t lvgl_scenario8_get_glyph_overflow_count(void)
{
    return 0;
}

void lvgl_scenario8_gpu2d_edges_create(void)
{
    printf("LVGL_SCENARIO8: requires LV_USE_DRAW_GPU_RENDERER\n");
}

#endif
