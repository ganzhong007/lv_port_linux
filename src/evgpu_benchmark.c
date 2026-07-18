#include <stdio.h>
#include "lvgl/lvgl.h"
#include "lvgl/lvgl_private.h"
#include "demo_g3_showcase.h"

#if LV_USE_PERF_MONITOR

#define SCENE_TIME_MS    4000
#define MAX_SCENES       13

typedef struct {
    const char * name;
    void (*create_cb)(lv_obj_t * parent);
    uint32_t cpu_avg_usage;
    uint32_t fps_avg;
    uint32_t render_avg_time;
    uint32_t flush_avg_time;
    uint32_t measurement_cnt;
} scene_dsc_t;

static scene_dsc_t scenes[MAX_SCENES];
static int scene_act = 0;
static lv_timer_t * scene_timer = NULL;
static lv_obj_t * scene_parent = NULL;
static lv_obj_t * info_label = NULL;

/******************** SCENE CREATORS ********************/

/* 1. FILL */
static void fill_scene_create(lv_obj_t * parent)
{
    int cols = 16, rows = 10;
    int w = lv_display_get_horizontal_resolution(NULL);
    int h = lv_display_get_vertical_resolution(NULL);
    int cw = w / cols;
    int rh = h / rows;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            lv_obj_t * obj = lv_obj_create(parent);
            lv_obj_set_pos(obj, c * cw, r * rh);
            lv_obj_set_size(obj, cw - 2, rh - 2);
            lv_obj_set_style_bg_color(obj, lv_palette_main(LV_PALETTE_RED + ((r * cols + c) % 10)), 0);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(obj, 0, 0);
            lv_obj_set_style_radius(obj, 0, 0);
            lv_obj_set_style_pad_all(obj, 0, 0);
            lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
        }
    }
}

/* 2. BORDER */
static void border_scene_create(lv_obj_t * parent)
{
    int cols = 16, rows = 10;
    int w = lv_display_get_horizontal_resolution(NULL);
    int h = lv_display_get_vertical_resolution(NULL);
    int cw = w / cols;
    int rh = h / rows;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            lv_obj_t * obj = lv_obj_create(parent);
            lv_obj_set_pos(obj, c * cw, r * rh);
            lv_obj_set_size(obj, cw - 2, rh - 2);
            lv_obj_set_style_border_width(obj, 6, 0);
            lv_obj_set_style_border_color(obj, lv_palette_main(LV_PALETTE_RED + ((r * cols + c) % 10)), 0);
            lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(obj, 0, 0);
            lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
            lv_obj_set_style_pad_all(obj, 0, 0);
            lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
        }
    }
}

/* 3. BOX_SHADOW */
static void box_shadow_scene_create(lv_obj_t * parent)
{
    int cols = 16, rows = 10;
    int w = lv_display_get_horizontal_resolution(NULL);
    int h = lv_display_get_vertical_resolution(NULL);
    int cw = w / cols;
    int rh = h / rows;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            lv_obj_t * obj = lv_obj_create(parent);
            lv_obj_set_pos(obj, c * cw, r * rh);
            lv_obj_set_size(obj, cw - 10, rh - 10);
            lv_obj_set_style_bg_color(obj, lv_palette_main(LV_PALETTE_GREY + ((r * cols + c) % 3)), 0);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(obj, 0, 0);
            lv_obj_set_style_radius(obj, 4, 0);
            lv_obj_set_style_shadow_width(obj, 12, 0);
            lv_obj_set_style_shadow_color(obj, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_set_style_shadow_opa(obj, LV_OPA_60, 0);
            lv_obj_set_style_shadow_spread(obj, 2, 0);
            lv_obj_set_style_pad_all(obj, 0, 0);
            lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
        }
    }
}

/* 4. LABEL (covers LETTER internally) */
static void label_scene_create(lv_obj_t * parent)
{
    int cols = 12, rows = 8;
    int w = lv_display_get_horizontal_resolution(NULL);
    int h = lv_display_get_vertical_resolution(NULL);
    int cw = w / cols;
    int rh = h / rows;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            lv_obj_t * label = lv_label_create(parent);
            lv_label_set_text_fmt(label, "%c%c", 'A' + ((r * cols + c) % 26), 'a' + ((r * cols + c * 3) % 26));
            lv_obj_set_pos(label, c * cw + 4, r * rh + 4);
            lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_GREEN + (r % 5)), 0);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
        }
    }
}

/* 5. IMAGE */
static void image_scene_create(lv_obj_t * parent)
{
    LV_IMAGE_DECLARE(img_benchmark_lvgl_logo_rgb);

    int cols = 8, rows = 5;
    int w = lv_display_get_horizontal_resolution(NULL);
    int h = lv_display_get_vertical_resolution(NULL);
    int cw = w / cols;
    int rh = h / rows;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            lv_obj_t * img = lv_image_create(parent);
            lv_image_set_src(img, &img_benchmark_lvgl_logo_rgb);
            lv_obj_set_pos(img, c * cw + 4, r * rh + 4);
            if((r * cols + c) % 3 == 0) {
                lv_image_set_scale(img, 200);
            }
            else if((r * cols + c) % 3 == 1) {
                lv_image_set_scale(img, 80);
            }
        }
    }
}

/* 6. LINE */
static lv_point_precise_t * line_get_pts(int cw, int rh)
{
    static lv_point_precise_t pts[5];
    pts[0].x = 5;    pts[0].y = 5;
    pts[1].x = cw - 10; pts[1].y = 5;
    pts[2].x = cw - 10; pts[2].y = rh - 10;
    pts[3].x = 5;    pts[3].y = rh - 10;
    pts[4].x = 5;    pts[4].y = 5;
    return pts;
}

static void line_scene_create(lv_obj_t * parent)
{
    int cols = 16, rows = 10;
    int w = lv_display_get_horizontal_resolution(NULL);
    int h = lv_display_get_vertical_resolution(NULL);
    int cw = w / cols;
    int rh = h / rows;

    lv_point_precise_t * pts = line_get_pts(cw, rh);

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            lv_obj_t * line = lv_line_create(parent);
            lv_line_set_points(line, pts, 5);
            lv_obj_set_pos(line, c * cw, r * rh);
            lv_obj_set_size(line, cw - 2, rh - 2);
            lv_obj_set_style_line_width(line, 4, 0);
            lv_obj_set_style_line_color(line, lv_palette_main(LV_PALETTE_BLUE + (r % 5)), 0);
            lv_obj_set_style_line_opa(line, LV_OPA_COVER, 0);
        }
    }
}

/* 7. ARC */
static void arc_scene_create(lv_obj_t * parent)
{
    int cols = 8, rows = 5;
    int w = lv_display_get_horizontal_resolution(NULL);
    int h = lv_display_get_vertical_resolution(NULL);
    int cw = w / cols;
    int rh = h / rows;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            lv_obj_t * arc = lv_arc_create(parent);
            lv_obj_set_pos(arc, c * cw + 4, r * rh + 4);
            lv_obj_set_size(arc, cw - 12, rh - 12);
            lv_arc_set_range(arc, 0, 100);
            lv_arc_set_value(arc, ((r * cols + c) * 17) % 101);
            lv_arc_set_bg_angles(arc, 0, 360);
            lv_obj_set_style_arc_color(arc, lv_palette_main(LV_PALETTE_ORANGE + (r % 4)), LV_PART_INDICATOR);
            lv_obj_set_style_arc_width(arc, 6, 0);
        }
    }
}

/* 8. BLUR */
static void blur_event_cb(lv_event_t * e)
{
    if(lv_event_get_code(e) == LV_EVENT_DRAW_MAIN) {
        lv_layer_t * layer = lv_event_get_layer(e);
        lv_obj_t * obj = lv_event_get_target(e);
        lv_area_t area;
        lv_obj_get_coords(obj, &area);

        lv_draw_blur_dsc_t dsc;
        lv_draw_blur_dsc_init(&dsc);
        dsc.blur_radius = 16;
        dsc.corner_radius = 0;
        dsc.quality = LV_BLUR_QUALITY_SPEED;
        lv_draw_blur(layer, &dsc, &area);
    }
}

static void blur_scene_create(lv_obj_t * parent)
{
    int w = lv_display_get_horizontal_resolution(NULL);
    int h = lv_display_get_vertical_resolution(NULL);

    /* Background content to blur */
    lv_obj_t * bg = lv_label_create(parent);
    lv_label_set_text(bg, "LVGL EVGPU Benchmark\nBlur Test Scene\nBackground Text\n\n"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n0123456789\n\n"
                           "Lorem ipsum dolor sit amet, consectetur adipiscing elit.\n"
                           "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.");
    lv_obj_set_pos(bg, 10, 10);
    lv_obj_set_style_text_font(bg, &lv_font_montserrat_24, 0);

    /* Blur overlay rectangles */
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 6; c++) {
            lv_obj_t * blur_area = lv_obj_create(parent);
            lv_obj_set_pos(blur_area, 20 + c * (w / 6), 20 + r * (h / 8));
            lv_obj_set_size(blur_area, w / 6 - 8, h / 8 - 8);
            lv_obj_set_style_bg_opa(blur_area, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(blur_area, 0, 0);
            lv_obj_add_event_cb(blur_area, blur_event_cb, LV_EVENT_DRAW_MAIN, NULL);
        }
    }
}

/* 9. TRIANGLE */
static void triangle_event_cb(lv_event_t * e)
{
    if(lv_event_get_code(e) == LV_EVENT_DRAW_MAIN) {
        lv_layer_t * layer = lv_event_get_layer(e);
        lv_obj_t * obj = lv_event_get_target(e);
        lv_area_t area;
        lv_obj_get_coords(obj, &area);

        lv_draw_triangle_dsc_t dsc;
        lv_draw_triangle_dsc_init(&dsc);
        lv_coord_t ow = lv_area_get_width(&area);
        dsc.p[0].x = area.x1;           dsc.p[0].y = area.y2;
        dsc.p[1].x = area.x1 + ow / 2;  dsc.p[1].y = area.y1;
        dsc.p[2].x = area.x2;           dsc.p[2].y = area.y2;
        dsc.color = lv_palette_main(LV_PALETTE_DEEP_PURPLE);
        dsc.opa = LV_OPA_COVER;
        lv_draw_triangle(layer, &dsc);
    }
}

static void triangle_scene_create(lv_obj_t * parent)
{
    int cols = 16, rows = 10;
    int w = lv_display_get_horizontal_resolution(NULL);
    int h = lv_display_get_vertical_resolution(NULL);
    int cw = w / cols;
    int rh = h / rows;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            lv_obj_t * tri = lv_obj_create(parent);
            lv_obj_set_pos(tri, c * cw, r * rh);
            lv_obj_set_size(tri, cw - 2, rh - 2);
            lv_obj_set_style_bg_opa(tri, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(tri, 0, 0);
            lv_obj_add_event_cb(tri, triangle_event_cb, LV_EVENT_DRAW_MAIN, NULL);
            lv_obj_set_scrollbar_mode(tri, LV_SCROLLBAR_MODE_OFF);
        }
    }
}

/* 10. GRADIENT (FILL with gradient descriptor) */
static void gradient_scene_create(lv_obj_t * parent)
{
    int cols = 12, rows = 8;
    int w = lv_display_get_horizontal_resolution(NULL);
    int h = lv_display_get_vertical_resolution(NULL);
    int cw = w / cols;
    int rh = h / rows;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            lv_obj_t * obj = lv_obj_create(parent);
            lv_obj_set_pos(obj, c * cw, r * rh);
            lv_obj_set_size(obj, cw - 2, rh - 2);
            lv_obj_set_style_radius(obj, 0, 0);
            lv_obj_set_style_border_width(obj, 0, 0);
            lv_obj_set_style_pad_all(obj, 0, 0);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(obj, lv_palette_main(LV_PALETTE_RED + ((r * cols + c) % 10)), 0);
            lv_obj_set_style_bg_grad_color(obj, lv_palette_main(LV_PALETTE_CYAN + ((c * cols + r) % 10)), 0);
            lv_obj_set_style_bg_grad_dir(obj, (r + c) % 2 == 0 ? LV_GRAD_DIR_HOR : LV_GRAD_DIR_VER, 0);
            lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
        }
    }
}

/* 11. LAYER (semi-transparent overlay forces layer composition) */
static void layer_scene_create(lv_obj_t * parent)
{
    int cols = 8, rows = 5;
    int w = lv_display_get_horizontal_resolution(NULL);
    int h = lv_display_get_vertical_resolution(NULL);
    int cw = w / cols;
    int rh = h / rows;

    /* Background content */
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            lv_obj_t * bg = lv_obj_create(parent);
            lv_obj_set_pos(bg, c * cw, r * rh);
            lv_obj_set_size(bg, cw - 2, rh - 2);
            lv_obj_set_style_bg_color(bg, lv_palette_main(LV_PALETTE_RED + ((r * cols + c) % 10)), 0);
            lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(bg, 0, 0);
            lv_obj_set_style_radius(bg, 0, 0);

            lv_obj_t * lbl = lv_label_create(bg);
            lv_label_set_text_fmt(lbl, "%d", r * cols + c);
            lv_obj_center(lbl);
            lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        }
    }

    /* Semi-transparent overlay - forces layer composition */
    lv_obj_t * overlay = lv_obj_create(parent);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_size(overlay, w, h);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_make(0, 0, 255), 0);
    lv_obj_set_style_opa(overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_scrollbar_mode(overlay, LV_SCROLLBAR_MODE_OFF);

    /* Children inside overlay to increase layer complexity */
    for (int i = 0; i < 20; i++) {
        lv_obj_t * lbl = lv_label_create(overlay);
        lv_label_set_text_fmt(lbl, "LAYER %d", i);
        lv_obj_set_pos(lbl, (i * 37) % w, (i * 53) % h);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    }
}

/* 12. MASK_RECT */
static void mask_rect_event_cb(lv_event_t * e)
{
    if(lv_event_get_code(e) == LV_EVENT_DRAW_MAIN) {
        lv_layer_t * layer = lv_event_get_layer(e);
        lv_obj_t * obj = lv_event_get_target(e);

        lv_draw_mask_rect_dsc_t dsc;
        lv_draw_mask_rect_dsc_init(&dsc);
        lv_coord_t x = lv_obj_get_x(obj);
        lv_coord_t y = lv_obj_get_y(obj);
        lv_coord_t w = lv_obj_get_width(obj);
        lv_coord_t h = lv_obj_get_height(obj);
        dsc.area.x1 = x + 4;
        dsc.area.y1 = y + 4;
        dsc.area.x2 = x + w - 4;
        dsc.area.y2 = y + h - 4;
        dsc.radius = 8;
        dsc.keep_outside = 0;

        lv_draw_mask_rect(layer, &dsc);
    }
}

static void mask_rect_scene_create(lv_obj_t * parent)
{
    int cols = 16, rows = 10;
    int w = lv_display_get_horizontal_resolution(NULL);
    int h = lv_display_get_vertical_resolution(NULL);
    int cw = w / cols;
    int rh = h / rows;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            lv_obj_t * mask_holder = lv_obj_create(parent);
            lv_obj_set_pos(mask_holder, c * cw, r * rh);
            lv_obj_set_size(mask_holder, cw - 2, rh - 2);
            lv_obj_set_style_bg_color(mask_holder, lv_palette_main(LV_PALETTE_TEAL + ((r * cols + c) % 8)), 0);
            lv_obj_set_style_bg_opa(mask_holder, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(mask_holder, 0, 0);
            lv_obj_set_scrollbar_mode(mask_holder, LV_SCROLLBAR_MODE_OFF);
            lv_obj_add_event_cb(mask_holder, mask_rect_event_cb, LV_EVENT_DRAW_MAIN, NULL);
        }
    }
}

/* 13. G3 SHOWCASE */
static void g3_showcase_scene_create(lv_obj_t * parent)
{
    LV_UNUSED(parent);
    demo_g3_showcase_init();
}

/******************** SCENE DATA ********************/

static void init_scenes(void)
{
    scenes[0].name = "FILL";
    scenes[0].create_cb = fill_scene_create;
    scenes[1].name = "BORDER";
    scenes[1].create_cb = border_scene_create;
    scenes[2].name = "BOX_SHADOW";
    scenes[2].create_cb = box_shadow_scene_create;
    scenes[3].name = "LABEL";
    scenes[3].create_cb = label_scene_create;
    scenes[4].name = "IMAGE";
    scenes[4].create_cb = image_scene_create;
    scenes[5].name = "LINE";
    scenes[5].create_cb = line_scene_create;
    scenes[6].name = "ARC";
    scenes[6].create_cb = arc_scene_create;
    scenes[7].name = "BLUR";
    scenes[7].create_cb = blur_scene_create;
    scenes[8].name = "TRIANGLE";
    scenes[8].create_cb = triangle_scene_create;
    scenes[9].name = "GRADIENT";
    scenes[9].create_cb = gradient_scene_create;
    scenes[10].name = "LAYER";
    scenes[10].create_cb = layer_scene_create;
    scenes[11].name = "MASK_RECT";
    scenes[11].create_cb = mask_rect_scene_create;
    scenes[12].name = "G3_SHOWCASE";
    scenes[12].create_cb = g3_showcase_scene_create;
}

/******************** PERF OBSERVER ********************/

static void sysmon_perf_observer_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(observer);
    const lv_sysmon_perf_info_t * info = lv_subject_get_pointer(subject);

    if(info_label) {
        lv_label_set_text_fmt(info_label,
                              "%s: %" LV_PRIu32" FPS, %" LV_PRIu32 "%% CPU\n"
                              "render %" LV_PRIu32" ms + flush %" LV_PRIu32" ms",
                              scenes[scene_act].name,
                              info->calculated.fps, info->calculated.cpu,
                              info->calculated.render_avg_time, info->calculated.flush_avg_time);
    }

    if(scenes[scene_act].measurement_cnt != 0) {
        scenes[scene_act].cpu_avg_usage += info->calculated.cpu;
        scenes[scene_act].fps_avg += info->calculated.fps;
        scenes[scene_act].render_avg_time += info->calculated.render_avg_time;
        scenes[scene_act].flush_avg_time += info->calculated.flush_avg_time;
    }
    scenes[scene_act].measurement_cnt++;
}

/******************** SCENE TIMER ********************/

static void next_scene_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    scene_act++;

    if(scene_act >= MAX_SCENES) {
        if(info_label) {
            lv_obj_delete(info_label);
            info_label = NULL;
        }
        if(scene_parent) {
            lv_obj_delete(scene_parent);
            scene_parent = NULL;
        }

        lv_obj_t * table = lv_table_create(lv_screen_active());
        lv_obj_set_size(table, lv_pct(100), lv_pct(100));

        lv_table_set_column_width(table, 0, 160);
        lv_table_set_column_width(table, 1, 100);
        lv_table_set_column_width(table, 2, 100);
        lv_table_set_column_width(table, 3, 180);

        lv_table_set_cell_value(table, 0, 0, "Scene");
        lv_table_set_cell_value(table, 0, 1, "Avg FPS");
        lv_table_set_cell_value(table, 0, 2, "Avg CPU%");
        lv_table_set_cell_value(table, 0, 3, "Avg Render+Flush (ms)");

        uint32_t sum_fps = 0, sum_cpu = 0, sum_render = 0, sum_flush = 0;
        int valid = 0;

        for(int i = 0; i < MAX_SCENES; i++) {
            if(scenes[i].measurement_cnt > 1) {
                uint32_t cnt = scenes[i].measurement_cnt - 1;
                uint32_t fps = scenes[i].fps_avg / cnt;
                uint32_t cpu = scenes[i].cpu_avg_usage / cnt;
                uint32_t ren = scenes[i].render_avg_time / cnt;
                uint32_t flu = scenes[i].flush_avg_time / cnt;

                lv_table_set_cell_value_fmt(table, i + 1, 0, "%s", scenes[i].name);
                lv_table_set_cell_value_fmt(table, i + 1, 1, "%" LV_PRIu32, fps);
                lv_table_set_cell_value_fmt(table, i + 1, 2, "%" LV_PRIu32 "%%", cpu);
                lv_table_set_cell_value_fmt(table, i + 1, 3, "%" LV_PRIu32 " (%" LV_PRIu32 "+%" LV_PRIu32 ")", ren + flu, ren, flu);

                printf("BENCH_RESULT %s FPS=%" LV_PRIu32 " CPU=%" LV_PRIu32 "%% R=%" LV_PRIu32 "ms F=%" LV_PRIu32 "ms\n",
                       scenes[i].name, fps, cpu, ren, flu);
                fflush(stdout);

                sum_fps += fps;
                sum_cpu += cpu;
                sum_render += ren;
                sum_flush += flu;
                valid++;
            }
        }

        if(valid > 0) {
            printf("BENCH_RESULT AVERAGE FPS=%" LV_PRIu32 " CPU=%" LV_PRIu32 "%% R+F=%" LV_PRIu32 "ms\n",
                   sum_fps / valid, sum_cpu / valid, (sum_render + sum_flush) / valid);
            fflush(stdout);
            lv_table_set_cell_value_fmt(table, valid + 1, 0, "AVERAGE");
            lv_table_set_cell_value_fmt(table, valid + 1, 1, "%" LV_PRIu32, sum_fps / valid);
            lv_table_set_cell_value_fmt(table, valid + 1, 2, "%" LV_PRIu32 "%%", sum_cpu / valid);
            lv_table_set_cell_value_fmt(table, valid + 1, 3, "%" LV_PRIu32 " ms",
                                        (sum_render + sum_flush) / valid);
        }

        lv_timer_delete(scene_timer);
        scene_timer = NULL;
        return;
    }

    lv_obj_t * scr = lv_screen_active();
    if(scene_parent) {
        lv_obj_delete(scene_parent);
        scene_parent = NULL;
    } else {
        /* Clean up scenes that built directly on the screen (e.g. G3 showcase) */
        uint32_t child_cnt = lv_obj_get_child_count(scr);
        lv_obj_t ** children = malloc(child_cnt * sizeof(lv_obj_t *));
        if(children) {
            for(uint32_t i = 0; i < child_cnt; i++)
                children[i] = lv_obj_get_child(scr, i);
            for(uint32_t i = 0; i < child_cnt; i++) {
                if(children[i] != info_label)
                    lv_obj_delete(children[i]);
            }
            free(children);
        }
    }

    scene_parent = lv_obj_create(scr);
    lv_obj_set_size(scene_parent, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(scene_parent, 0, 30);
    lv_obj_set_style_bg_opa(scene_parent, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scene_parent, 0, 0);
    lv_obj_set_scrollbar_mode(scene_parent, LV_SCROLLBAR_MODE_OFF);

    scenes[scene_act].create_cb(scene_parent);

    lv_timer_set_period(scene_timer, SCENE_TIME_MS);
    lv_timer_set_repeat_count(scene_timer, 1);
}

/******************** ENTRY POINT ********************/

void evgpu_benchmark_create(void)
{
    init_scenes();
    scene_act = 0;

    lv_obj_t * scr = lv_screen_active();

    info_label = lv_label_create(scr);
    lv_obj_set_pos(info_label, 0, 0);
    lv_obj_set_size(info_label, lv_pct(100), 30);
    lv_label_set_text(info_label, "Starting...");

    scene_parent = lv_obj_create(scr);
    lv_obj_set_size(scene_parent, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(scene_parent, 0, 30);
    lv_obj_set_style_bg_opa(scene_parent, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scene_parent, 0, 0);
    lv_obj_set_scrollbar_mode(scene_parent, LV_SCROLLBAR_MODE_OFF);

    lv_display_t * disp = lv_display_get_default();
    lv_subject_add_observer_obj(&disp->perf_sysmon_backend.subject, sysmon_perf_observer_cb, NULL, NULL);

    scenes[0].create_cb(scene_parent);

    scene_timer = lv_timer_create(next_scene_timer_cb, SCENE_TIME_MS, NULL);
    lv_timer_set_repeat_count(scene_timer, 1);
}

#else
void evgpu_benchmark_create(void)
{
    lv_obj_t * label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "ERROR: LV_USE_PERF_MONITOR is not enabled in lv_conf.h");
    lv_obj_center(label);
}
#endif
