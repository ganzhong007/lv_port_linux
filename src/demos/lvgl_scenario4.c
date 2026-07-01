/**
 * @file lvgl_scenario4.c — 场景四：圆角立体 3D 按钮（Quick3D 式 hover 抬起 + click 下压）
 */

#include "lvgl/lvgl.h"
#include "lvgl_demos.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS && LV_USE_3DBUTTON && LV_USE_DRAW_GPU_RENDERER

#include "draw/gpu_renderer/lv_draw_gpu_renderer.h"

#include <stdio.h>

#define S4_BTN_W         300.0f
#define S4_BTN_H          18.0f
#define S4_BTN_DEPTH      72.0f
#define S4_BTN_RADIUS     16.0f
#define S4_BTN_PITCH       0.0f
#define S4_BTN_YAW         0.0f
#define S4_BTN_Z        (-280.0f)

#define S4_HOVER_LIFT     12.0f
#define S4_HOVER_SCALE   1.035f
#define S4_PRESS_SINK    (-16.0f)
#define S4_PRESS_SCALE    0.96f

static lv_obj_t * g_vp;
static lv_obj_t * g_btn;
static lv_obj_t * g_count_label;
static uint32_t g_click_count;

static void btn_click_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    g_click_count++;
    if(g_count_label) {
        lv_label_set_text_fmt(g_count_label, "3D button clicks: %u", (unsigned)g_click_count);
    }
    printf("LVGL_SCENARIO4: clicked count=%u\n", (unsigned)g_click_count);
}

void lvgl_scenario4_3dbutton_create(void)
{
    lv_gpu_renderer_set_ui_mode(LV_GPU_RENDERER_UI_GENERIC);

    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_opa(scr, LV_OPA_0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * cam = lv_3dcamera_create(scr);
    lv_3dcamera_set_perspective(cam, 36.0f, 12.0f, 5000.0f);
    /* Elevated view so the +Y cap (top face) is visible; gray 3D clear avoids black passthrough. */
    lv_3dcamera_look_at(cam,
                        (lv_vec3_t) { 0.0f, 180.0f, 520.0f },
                        (lv_vec3_t) { 0.0f, 0.0f, S4_BTN_Z },
                        (lv_vec3_t) { 0.0f, 1.0f, 0.0f });

    lv_obj_t * scene = lv_3dscene_create(scr);
    lv_obj_set_style_bg_opa(scene, LV_OPA_0, 0);

    g_vp = lv_3dviewport_create(scr);
    lv_obj_set_size(g_vp, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(g_vp, LV_OPA_0, 0);
    lv_3dviewport_set_camera(g_vp, cam);
    lv_3dviewport_set_scene(g_vp, scene);
    lv_3dviewport_set_pickable(g_vp, true);
    lv_3dviewport_set_input_routing(g_vp, true);

    g_btn = lv_3dbutton_create(scene);
    lv_3dbutton_set_box_size(g_btn, S4_BTN_W, S4_BTN_H, S4_BTN_DEPTH);
    lv_3dbutton_set_corner_radius(g_btn, S4_BTN_RADIUS);
    lv_3dbutton_set_colors(g_btn, lv_color_hex(0x1E88E5), lv_color_hex(0x0D47A1));
    lv_3dbutton_set_hover_lift(g_btn, S4_HOVER_LIFT, S4_HOVER_SCALE);
    lv_3dbutton_set_press_depth(g_btn, S4_PRESS_SINK, S4_PRESS_SCALE);
    lv_3dbutton_set_tilt(g_btn, S4_BTN_PITCH, S4_BTN_YAW);
    lv_3dbutton_place(g_btn, 0.0f, 0.0f, S4_BTN_Z);
    lv_obj_add_event_cb(g_btn, btn_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * panel = lv_obj_create(scr);
    lv_obj_set_size(panel, 340, 56);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_set_style_bg_opa(panel, LV_OPA_70, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    g_count_label = lv_label_create(panel);
    lv_label_set_text(g_count_label, "3D button clicks: 0");
    lv_obj_center(g_count_label);

    lv_obj_t * hint = lv_label_create(scr);
    lv_label_set_text(hint, "Hover to lift  |  Click to press");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -12);

    lv_obj_invalidate(g_vp);
}

#else

void lvgl_scenario4_3dbutton_create(void)
{
    printf("LVGL_SCENARIO4: requires LV_USE_3D + LV_USE_3DBUTTON + LV_USE_DRAW_GPU_RENDERER\n");
}

#endif
