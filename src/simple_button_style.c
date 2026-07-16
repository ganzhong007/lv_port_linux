/**
 * @file simple_button_style.c
 * @brief Minimal example: tap a button to cycle LVGL themes.
 *
 * Flow:
 *   CLICKED → pick next theme (default light/dark, simple, mono)
 *           → lv_display_set_theme()
 *           → rebuild screen (set_theme does not re-skin existing widgets)
 *
 * Widget look comes only from theme apply at create time — no local styles.
 */
#include "lvgl/lvgl.h"
#include "simple_button_style.h"
#include "lvgl_port_trace.h"

typedef enum {
    THEME_DEFAULT_LIGHT = 0,
    THEME_DEFAULT_DARK,
    THEME_SIMPLE,
    THEME_MONO,
    THEME_COUNT
} theme_mode_t;

static theme_mode_t s_theme_mode = THEME_DEFAULT_LIGHT;
static lv_obj_t * s_title_label;

static const char * theme_mode_name(theme_mode_t mode)
{
    switch(mode) {
        case THEME_DEFAULT_LIGHT:
            return "Theme: default light";
        case THEME_DEFAULT_DARK:
            return "Theme: default dark";
        case THEME_SIMPLE:
            return "Theme: simple";
        case THEME_MONO:
            return "Theme: mono";
        default:
            return "Theme: ?";
    }
}

static void apply_theme_mode(theme_mode_t mode)
{
    lv_display_t * disp = lv_display_get_default();
    lv_theme_t * th = NULL;

    switch(mode) {
        case THEME_DEFAULT_LIGHT:
            th = lv_theme_default_init(disp,
                                       lv_palette_main(LV_PALETTE_BLUE),
                                       lv_palette_main(LV_PALETTE_RED),
                                       false, LV_FONT_DEFAULT);
            break;
        case THEME_DEFAULT_DARK:
            th = lv_theme_default_init(disp,
                                       lv_palette_main(LV_PALETTE_BLUE),
                                       lv_palette_main(LV_PALETTE_RED),
                                       true, LV_FONT_DEFAULT);
            break;
        case THEME_SIMPLE:
            th = lv_theme_simple_init(disp);
            break;
        case THEME_MONO:
            th = lv_theme_mono_init(disp, true, LV_FONT_DEFAULT);
            break;
        default:
            break;
    }

    if(th) {
        lv_display_set_theme(disp, th);
        LVGL_PORT_TRACE("L1-APP", "lv_display_set_theme -> %s", theme_mode_name(mode));
    }
}

static void rebuild_ui(void);

static void on_theme_btn_clicked(lv_event_t * e)
{
    LV_UNUSED(e);

    s_theme_mode = (theme_mode_t)((s_theme_mode + 1) % THEME_COUNT);
    LVGL_PORT_TRACE("L1-APP", "CLICKED -> switch theme to %s", theme_mode_name(s_theme_mode));

    apply_theme_mode(s_theme_mode);
    /* Theme styles live in obj->styles[]; set_theme alone does not re-skin
     * existing widgets (see lv_display_set_theme). Rebuild so new widgets
     * call lv_theme_apply() from lv_obj_class_init_obj(). */
    rebuild_ui();
}

static void rebuild_ui(void)
{
    lv_obj_t * scr = lv_screen_active();
    lv_obj_clean(scr);

    s_title_label = lv_label_create(scr);
    lv_label_set_text(s_title_label, theme_mode_name(s_theme_mode));
    lv_obj_align(s_title_label, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t * hint = lv_label_create(scr);
    lv_label_set_text(hint, "Tap the button to cycle themes");
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 80);

    lv_obj_t * btn = lv_button_create(scr);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 130);
    lv_obj_add_event_cb(btn, on_theme_btn_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Change theme");
    lv_obj_center(btn_label);

    lv_obj_t * sw_caption = lv_label_create(scr);
    lv_label_set_text(sw_caption, "Switch");
    lv_obj_align(sw_caption, LV_ALIGN_TOP_MID, -60, 220);

    lv_obj_t * sw = lv_switch_create(scr);
    lv_obj_align(sw, LV_ALIGN_TOP_MID, 40, 215);
    lv_obj_add_state(sw, LV_STATE_CHECKED);

    lv_obj_t * slider_caption = lv_label_create(scr);
    lv_label_set_text(slider_caption, "Slider");
    lv_obj_align(slider_caption, LV_ALIGN_TOP_MID, 0, 280);

    lv_obj_t * slider = lv_slider_create(scr);
    lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 320);
    lv_slider_set_value(slider, 60, LV_ANIM_OFF);
}

void simple_button_style_create(void)
{
    s_theme_mode = THEME_DEFAULT_LIGHT;
    apply_theme_mode(s_theme_mode);
    rebuild_ui();
    LVGL_PORT_TRACE("L1-APP", "simple_button_style ready");
}
