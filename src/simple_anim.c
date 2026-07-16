/**
 * @file simple_anim.c
 * @brief Minimal example: lv_anim drives an object's x position.
 *
 * Flow:
 *   lv_anim_init / set_var / set_values / set_exec_cb / set_path_cb
 *     → lv_anim_start()
 *     → each tick: exec_cb(obj, interpolated_x)
 *     → reverse + infinite repeat
 */
#include "lvgl/lvgl.h"
#include "simple_anim.h"
#include "lvgl_port_trace.h"

static void anim_x_cb(void * var, int32_t v)
{
    lv_obj_set_x((lv_obj_t *)var, v);
}

void simple_anim_create(void)
{
    lv_obj_t * scr = lv_screen_active();

    lv_obj_t * title = lv_label_create(scr);
    lv_label_set_text(title, "Simple anim: slides left <-> right");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t * box = lv_obj_create(scr);
    lv_obj_set_size(box, 64, 64);
    lv_obj_align(box, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * label = lv_label_create(box);
    lv_label_set_text(label, "Go");
    lv_obj_center(label);

    const int32_t x_start = 20;
    const int32_t x_end = lv_display_get_horizontal_resolution(NULL) - 84;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, box);
    lv_anim_set_values(&a, x_start, x_end);
    lv_anim_set_duration(&a, 1200);
    lv_anim_set_reverse_duration(&a, 1200);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, anim_x_cb);
    lv_anim_start(&a);

    LVGL_PORT_TRACE("L1-APP", "simple_anim ready (x %d -> %d)", (int)x_start, (int)x_end);
}
