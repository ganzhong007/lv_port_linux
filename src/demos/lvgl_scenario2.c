/**
 * @file lvgl_scenario2.c — 场景二：静态透明底 + 线框楼群 (Phase 1)
 */

#include "lvgl/lvgl.h"
#include "lvgl_demos.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS

void lvgl_scenario2_skyline_create(void)
{
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_opa(scr, LV_OPA_0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * cam = lv_3dcamera_create(scr);
    lv_3dcamera_set_perspective(cam, 50.0f, 10.0f, 8000.0f);
    lv_3dcamera_look_at(cam,
                        (lv_vec3_t) { 0, 350, 1200 },
                        (lv_vec3_t) { 0, 80, 0 },
                        (lv_vec3_t) { 0, 1, 0 });

    lv_obj_t * scene = lv_3dscene_create(scr);
    lv_obj_t * vp = lv_3dviewport_create(scr);
    lv_obj_set_size(vp, LV_PCT(100), LV_PCT(100));
    lv_3dviewport_set_camera(vp, cam);
    lv_3dviewport_set_scene(vp, scene);

    static const struct { float x, z, w, h, d; } buildings[] = {
        {-420, -200, 90, 220, 90}, {-280, -350, 70, 160, 70}, {-140, -500, 110, 280, 100},
        {140, -480, 85, 200, 85}, {300, -320, 95, 240, 95}, {450, -180, 75, 170, 75},
        {-60, -650, 130, 320, 110}, {80, -620, 100, 260, 90},
        {0, -300, 60, 140, 60},
    };

    for(size_t i = 0; i < sizeof(buildings) / sizeof(buildings[0]); i++) {
        lv_obj_t * m = lv_3dmesh_create(scene);
        lv_3dmesh_set_box(m, buildings[i].w, buildings[i].h, buildings[i].d);
        lv_3dmesh_set_wireframe(m, true);
        lv_3dmesh_set_color(m, lv_color_hex(0x00FFAA));
        lv_3dmesh_set_position(m, buildings[i].x, buildings[i].h * 0.5f, buildings[i].z);
    }
}

#else

void lvgl_scenario2_skyline_create(void) {}

#endif
