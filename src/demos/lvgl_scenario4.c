/**
 * @file lvgl_scenario4.c — 场景四：3D 按钮（lv_3dbutton + viewport 输入路由）
 */

#include "lvgl/lvgl.h"
#include "lvgl_demos.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS && LV_USE_3DBUTTON && LV_USE_DRAW_GPU_COMPOSITE

#include "draw/gpu_composite/lv_draw_gpu_composite.h"

#include <stdio.h>

/** Button footprint (scene units, Z = extrusion depth). */
#define S4_BTN_W      220.0f
#define S4_BTN_H      116.0f
#define S4_BTN_DEPTH  224.0f
#define S4_BTN_PITCH   (-5.0f)
#define S4_BTN_YAW       8.0f
#define S4_BTN_Z     (-300.0f)

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
    lv_gpu_composite_set_ui_mode(LV_GPU_COMPOSITE_UI_GENERIC);

    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_opa(scr, LV_OPA_0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * cam = lv_3dcamera_create(scr);
    lv_3dcamera_set_perspective(cam, 38.0f, 10.0f, 5000.0f);
    /* Oblique eye — exposes button side faces (not head-on). */
    lv_3dcamera_look_at(cam,
                        (lv_vec3_t) { 260.0f, 160.0f, 760.0f },
                        (lv_vec3_t) { 0.0f, -8.0f, S4_BTN_Z },
                        (lv_vec3_t) { 0.0f, 1.0f, 0.0f });

    lv_obj_t * scene = lv_3dscene_create(scr);

    g_vp = lv_3dviewport_create(scr);
    lv_obj_set_size(g_vp, lv_pct(100), lv_pct(100));
    lv_3dviewport_set_camera(g_vp, cam);
    lv_3dviewport_set_scene(g_vp, scene);
    lv_3dviewport_set_pickable(g_vp, true);
    lv_3dviewport_set_input_routing(g_vp, true);

    g_btn = lv_3dbutton_create(scene);
    lv_3dbutton_set_box_size(g_btn, S4_BTN_W, S4_BTN_H, S4_BTN_DEPTH);
    lv_3dbutton_set_colors(g_btn, lv_color_hex(0x1E88E5), lv_color_hex(0x0D47A1));
    lv_3dbutton_set_tilt(g_btn, S4_BTN_PITCH, S4_BTN_YAW);
    lv_3dmesh_set_position(g_btn, 0.0f, 0.0f, S4_BTN_Z);
    lv_obj_add_event_cb(g_btn, btn_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * panel = lv_obj_create(scr);
    lv_obj_set_size(panel, 300, 56);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_set_style_bg_opa(panel, LV_OPA_70, 0);
    lv_obj_set_style_radius(panel, 5, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    g_count_label = lv_label_create(panel);
    lv_label_set_text(g_count_label, "3D button clicks: 0");
    lv_obj_center(g_count_label);

    lv_obj_t * hint = lv_label_create(scr);
    lv_label_set_text_fmt(hint,
                          "Extruded 3D button  %.0f x %.0f x %.0f  tilt(%.0f, %.0f)",
                          S4_BTN_W, S4_BTN_H, S4_BTN_DEPTH, S4_BTN_PITCH, S4_BTN_YAW);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -12);
}

#else

void lvgl_scenario4_3dbutton_create(void)
{
    printf("LVGL_SCENARIO4: requires LV_USE_3D + LV_USE_3DBUTTON + LV_USE_DRAW_GPU_COMPOSITE\n");
}

#endif
