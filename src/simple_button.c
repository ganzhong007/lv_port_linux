/**
 * @file simple_button.c
 * @brief Application layer (L1): create one button and handle click events.
 */
#include "lvgl/lvgl.h"
#include "simple_button.h"
#include "simple_button_shot.h"
#include "lvgl_port_trace.h"
#include <stdlib.h>
#include <stdio.h>

static char doc_shots_dir[512];

static void doc_shot_after_timer_cb(lv_timer_t * t)
{
    LV_UNUSED(t);
    char path[640];
    snprintf(path, sizeof(path), "%s/simple_button_after.ppm", doc_shots_dir);
    simple_button_save_screen_png(path);
    exit(0);
}

static void doc_auto_click_timer_cb(lv_timer_t * t)
{
    lv_obj_t * btn = lv_timer_get_user_data(t);
    lv_obj_send_event(btn, LV_EVENT_CLICKED, NULL);
    lv_timer_create(doc_shot_after_timer_cb, 300, NULL);
}

static void doc_shot_before_timer_cb(lv_timer_t * t)
{
    lv_obj_t * btn = lv_timer_get_user_data(t);
    char path[640];
    snprintf(path, sizeof(path), "%s/simple_button_before.ppm", doc_shots_dir);
    simple_button_save_screen_png(path);
    lv_timer_create(doc_auto_click_timer_cb, 700, btn);
}

static void btn_event_cb(lv_event_t * e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target_obj(e);

    switch(code) {
        case LV_EVENT_PRESSED:
            LVGL_PORT_TRACE("L1-APP", "LV_EVENT_PRESSED on button %p", (void *)btn);
            break;
        case LV_EVENT_RELEASED:
            LVGL_PORT_TRACE("L1-APP", "LV_EVENT_RELEASED on button %p", (void *)btn);
            break;
        case LV_EVENT_CLICKED:
            LVGL_PORT_TRACE("L1-APP", "LV_EVENT_CLICKED -> update label text");
            {
                lv_obj_t * label = lv_obj_get_child(btn, 0);
                lv_label_set_text(label, "Clicked!");
            }
            break;
        default:
            break;
    }
}

void simple_button_create(void)
{
    lv_obj_t * scr = lv_screen_active();

    LVGL_PORT_TRACE("L1-APP", "create 1/8-screen button on screen %p", (void *)scr);

    lv_obj_t * btn = lv_button_create(scr);
    lv_obj_set_size(btn,
                    lv_display_get_horizontal_resolution(NULL) / 8,
                    lv_display_get_vertical_resolution(NULL) / 8);
    lv_obj_center(btn);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "Hello LVGL");
    lv_obj_center(label);

    LVGL_PORT_TRACE("L1-APP", "button ready; first invalidate/refresh follows from lv_timer_handler()");

    const char * shots = getenv("SIMPLE_BUTTON_DOC_SHOTS");
    if(shots != NULL && shots[0] != '\0') {
        lv_snprintf(doc_shots_dir, sizeof(doc_shots_dir), "%s", shots);
        lv_timer_create(doc_shot_before_timer_cb, 500, btn);
    }
}
