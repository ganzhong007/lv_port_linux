/**
 * @file lvgl_verify.c — 逐帧验收（corner alpha + 全屏采样像素内容）
 */

#include "lvgl/lvgl.h"
#include "lvgl_demos.h"

#if LV_USE_DRAW_GPU_COMPOSITE
#include "draw/gpu_composite/lv_draw_gpu_composite.h"
#endif

#include <stdio.h>
#include <stdlib.h>

static int scenario_id;
static int verify_done;
static int warmup_left;
static int frames_to_check;
static int frames_checked;
static int frames_seen;
static uint32_t last_verified_serial;

#if LV_USE_3D && LV_USE_DRAW_GPU_COMPOSITE

static int env_int(const char * name, int default_val)
{
    const char * v = getenv(name);
    if(!v || !v[0]) return default_val;
    return atoi(v);
}

static int verify_frame_content(int frame_idx, const lv_gpu_composite_verify_stats_t * s)
{
    if(s->last_flush_viewports < 1) {
        printf("LVGL_VERIFY: FAIL scenario=%d frame=%d no viewport flush\n", scenario_id, frame_idx);
        return 0;
    }

    if(scenario_id == 2) {
        if(s->last_flush_items < 9) {
            printf("LVGL_VERIFY: FAIL scenario=%d frame=%d flush_items=%u (expect >=9)\n",
                   scenario_id, frame_idx, (unsigned)s->last_flush_items);
            return 0;
        }
        if(s->region_max_alpha < 32 && s->flush_max_alpha < 32) {
            printf("LVGL_VERIFY: FAIL scenario=%d frame=%d max_alpha=%u flush_max=%u (expect visible 3D pixels)\n",
                   scenario_id, frame_idx, (unsigned)s->region_max_alpha, (unsigned)s->flush_max_alpha);
            return 0;
        }
        if(s->region_greenish_count < 1 && s->region_visible_count < 8) {
            printf("LVGL_VERIFY: FAIL scenario=%d frame=%d greenish=%u visible=%u flush_max=%u\n",
                   scenario_id, frame_idx,
                   (unsigned)s->region_greenish_count, (unsigned)s->region_visible_count,
                   (unsigned)s->flush_max_alpha);
            return 0;
        }
        return 1;
    }

    if(scenario_id == 1) {
        if(s->last_flush_items < 9) {
            printf("LVGL_VERIFY: FAIL scenario=%d frame=%d flush_items=%u (expect >=9 tiles)\n",
                   scenario_id, frame_idx, (unsigned)s->last_flush_items);
            return 0;
        }
        if(s->region_max_alpha < 128 && s->flush_max_alpha < 128) {
            printf("LVGL_VERIFY: FAIL scenario=%d frame=%d max_alpha=%u flush_max=%u (expect opaque tiles)\n",
                   scenario_id, frame_idx, (unsigned)s->region_max_alpha, (unsigned)s->flush_max_alpha);
            return 0;
        }
        if(s->region_opaque_count < 32) {
            printf("LVGL_VERIFY: FAIL scenario=%d frame=%d opaque=%u colorful=%u\n",
                   scenario_id, frame_idx,
                   (unsigned)s->region_opaque_count, (unsigned)s->region_colorful_count);
            return 0;
        }
        if(s->region_colorful_count < 32) {
            printf("LVGL_VERIFY: FAIL scenario=%d frame=%d colorful=%u (expect colored tiles)\n",
                   scenario_id, frame_idx, (unsigned)s->region_colorful_count);
            return 0;
        }
        return 1;
    }

    printf("LVGL_VERIFY: FAIL scenario=%d frame=%d (unknown scenario)\n", scenario_id, frame_idx);
    return 0;
}

static void verify_one_frame(lv_display_t * disp)
{
    if(verify_done) return;

    frames_seen++;

    if(warmup_left > 0) {
        warmup_left--;
        return;
    }

    if(frames_checked >= frames_to_check) return;

    const int frame_idx = frames_checked + 1;

    lv_gpu_composite_verify_stats_t stats;
    if(!lv_gpu_composite_verify_stats(disp, &stats)) {
        printf("LVGL_VERIFY: FAIL scenario=%d frame=%d (readback failed)\n", scenario_id, frame_idx);
        verify_done = 1;
        exit(1);
    }

    if(stats.last_flush_items < 1) {
        printf("LVGL_VERIFY: FAIL scenario=%d frame=%d no 3D draw (warmup too short?)\n",
               scenario_id, frame_idx);
        verify_done = 1;
        exit(1);
    }

    if(stats.flush_serial == last_verified_serial) return;
    last_verified_serial = stats.flush_serial;

    if(stats.corner_min_alpha > 10) {
        printf("LVGL_VERIFY: FAIL scenario=%d frame=%d corner_min_alpha=%u (expect ~0 AR passthrough)\n",
               scenario_id, frame_idx, (unsigned)stats.corner_min_alpha);
        verify_done = 1;
        exit(1);
    }

    if(!verify_frame_content(frame_idx, &stats)) {
        verify_done = 1;
        exit(1);
    }

    frames_checked++;
    printf("LVGL_VERIFY: frame %d/%d ok corner_a=%u max_a=%u flush_max=%u flush_items=%u visible=%u\n",
           frame_idx, frames_to_check,
           (unsigned)stats.corner_min_alpha,
           (unsigned)stats.region_max_alpha,
           (unsigned)stats.flush_max_alpha,
           (unsigned)stats.last_flush_items,
           (unsigned)stats.region_visible_count);

    if(frames_checked >= frames_to_check) {
        printf("LVGL_VERIFY: PASS scenario=%d checked_frames=%d seen_frames=%d samples_per_frame=%u\n",
               scenario_id, frames_to_check, frames_seen, (unsigned)stats.region_samples);
        verify_done = 1;
        exit(0);
    }
}

static void verify_frame_cb(lv_display_t * disp)
{
    verify_one_frame(disp);
}

static void verify_timer_cb(lv_timer_t * t)
{
    LV_UNUSED(t);
    if(verify_done) return;
    lv_obj_invalidate(lv_screen_active());
}

int lvgl_verify_run(int id)
{
    scenario_id = id;
    verify_done = 0;
    frames_seen = 0;
    frames_checked = 0;
    last_verified_serial = 0;
    warmup_left = env_int("LVGL_VERIFY_WARMUP", 8);
    frames_to_check = env_int("LVGL_VERIFY_FRAMES", 60);
    if(frames_to_check < 1) frames_to_check = 1;

    printf("LVGL_VERIFY: start scenario=%d warmup=%d frames=%d\n",
           scenario_id, warmup_left, frames_to_check);

    lv_gpu_composite_set_frame_ready_cb(verify_frame_cb);
    lv_timer_create(verify_timer_cb, 16, NULL);
    return 0;
}

#else

int lvgl_verify_run(int id)
{
    LV_UNUSED(id);
    printf("LVGL_VERIFY: SKIP (gpu_composite not enabled)\n");
    return 0;
}

#endif
