/**
 * @file lvgl_verify.c — 逐帧验收（corner alpha + 全屏采样像素内容）
 */

#include "lvgl/lvgl.h"
#include "lvgl_demos.h"

#if LV_USE_3D && LV_USE_DRAW_GPU_RENDERER
#include "draw/gpu_renderer/lv_draw_gpu_renderer.h"
#if LV_USE_SNAPSHOT
#include "lvgl/3d/lv_3d_plane_bake.h"
#endif
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
static float s2_min_z_first;
static float s2_min_z_last;

#if LV_USE_3D && LV_USE_DRAW_GPU_RENDERER

static int env_int(const char * name, int default_val)
{
    const char * v = getenv(name);
    if(!v || !v[0]) return default_val;
    return atoi(v);
}

static int env_bool(const char * name, int default_val)
{
    const char * v = getenv(name);
    if(!v || !v[0]) return default_val;
    return atoi(v) != 0;
}

static void verify_log_path_stats(const lv_gpu_renderer_verify_stats_t * s)
{
    if(s->gl_renderer[0]) {
        printf("LVGL_VERIFY: path renderer=%s gpu2d=%u gpu3d=%u sw_overlay=%u sw_raster=%u "
               "fg_pass=%u fg_batch=%u fg_mat=%u gl_finish=%u\n",
               s->gl_renderer,
               (unsigned)s->gpu_2d_tasks,
               (unsigned)s->gpu_3d_draws,
               (unsigned)s->sw_overlay_uploads,
               (unsigned)s->sw_2d_raster_tasks,
               (unsigned)s->fg_pass_count,
               (unsigned)s->fg_batch_count,
               (unsigned)s->fg_material_batches,
               (unsigned)s->fg_gl_finish_count);
    }
    else {
        printf("LVGL_VERIFY: path gpu2d=%u gpu3d=%u sw_overlay=%u sw_raster=%u "
               "fg_pass=%u fg_batch=%u fg_mat=%u gl_finish=%u\n",
               (unsigned)s->gpu_2d_tasks,
               (unsigned)s->gpu_3d_draws,
               (unsigned)s->sw_overlay_uploads,
               (unsigned)s->sw_2d_raster_tasks,
               (unsigned)s->fg_pass_count,
               (unsigned)s->fg_batch_count,
               (unsigned)s->fg_material_batches,
               (unsigned)s->fg_gl_finish_count);
    }
}

static int verify_fg_framegraph(const lv_gpu_renderer_verify_stats_t * s)
{
    if(!env_bool("LVGL_VERIFY_FG", 1)) return 1;

    if(s->fg_gl_finish_count > 1) {
        printf("LVGL_VERIFY: FAIL fg scenario=%d gl_finish_count=%u (expect <=1)\n",
               scenario_id, (unsigned)s->fg_gl_finish_count);
        return 0;
    }

    switch(scenario_id) {
        case 1:
            /* AR launcher: 3D pass + optional 2D overlay */
            if(s->fg_pass_count < 1 || s->fg_pass_count > 3) {
                printf("LVGL_VERIFY: FAIL fg scenario=1 pass_count=%u (expect 1..3)\n",
                       (unsigned)s->fg_pass_count);
                return 0;
            }
            if(s->gpu_3d_draws < 1) {
                printf("LVGL_VERIFY: FAIL fg scenario=1 pure-3D component gpu_3d_draws=%u\n",
                       (unsigned)s->gpu_3d_draws);
                return 0;
            }
            break;
        case 2:
            /* NAV AR: mixed 3D + GPU 2D HUD */
            if(s->fg_pass_count < 1 || s->fg_pass_count > 3) {
                printf("LVGL_VERIFY: FAIL fg scenario=2 pass_count=%u (expect 1..3)\n",
                       (unsigned)s->fg_pass_count);
                return 0;
            }
            if(s->fg_batch_count < 1) {
                printf("LVGL_VERIFY: FAIL fg scenario=2 batch_count=%u (expect >=1)\n",
                       (unsigned)s->fg_batch_count);
                return 0;
            }
            break;
        case 3:
            /* 3D stress + small HUD labels */
            if(s->fg_pass_count < 1 || s->fg_pass_count > 3) {
                printf("LVGL_VERIFY: FAIL fg scenario=3 pass_count=%u (expect 1..3)\n",
                       (unsigned)s->fg_pass_count);
                return 0;
            }
            if(s->gpu_3d_draws < s->gpu_2d_tasks) {
                printf("LVGL_VERIFY: FAIL fg scenario=3 gpu_3d=%u < gpu_2d=%u (3D should dominate)\n",
                       (unsigned)s->gpu_3d_draws, (unsigned)s->gpu_2d_tasks);
                return 0;
            }
            break;
        case 4:
            if(s->fg_pass_count < 1 || s->fg_pass_count > 3) {
                printf("LVGL_VERIFY: FAIL fg scenario=4 pass_count=%u (expect 1..3)\n",
                       (unsigned)s->fg_pass_count);
                return 0;
            }
            if(s->gpu_3d_draws < 1) {
                printf("LVGL_VERIFY: FAIL fg scenario=4 gpu_3d_draws=%u (expect >=1 button mesh)\n",
                       (unsigned)s->gpu_3d_draws);
                return 0;
            }
            break;
        default:
            break;
    }
    return 1;
}

static int verify_gpu_path(const lv_gpu_renderer_verify_stats_t * s)
{
    if(!env_bool("LVGL_VERIFY_GPU_PATH", 0)) return 1;

    if(scenario_id == 2) {
        if(s->gpu_2d_tasks < 1) {
            printf("LVGL_VERIFY: FAIL gpu_path scenario=2 gpu_2d_tasks=%u (expect >=1)\n",
                   (unsigned)s->gpu_2d_tasks);
            return 0;
        }
        if(s->sw_overlay_uploads != 0) {
            printf("LVGL_VERIFY: FAIL gpu_path scenario=2 sw_overlay_uploads=%u (expect 0)\n",
                   (unsigned)s->sw_overlay_uploads);
            return 0;
        }
    }
    if(scenario_id == 3) {
        const int expect = lvgl_scenario3_get_mesh_count();
        const uint32_t min_draws = expect > 0 ? (uint32_t)(expect * 3 / 4) : 80u;
        if(s->gpu_3d_draws < min_draws) {
            printf("LVGL_VERIFY: FAIL gpu_path scenario=3 gpu_3d_draws=%u (expect >=%u)\n",
                   (unsigned)s->gpu_3d_draws, (unsigned)min_draws);
            return 0;
        }
    }
    return 1;
}

static int verify_lite_mode(void)
{
    return lv_gpu_renderer_verify_lite_enabled() ? 1 : 0;
}

static int verify_frame_content(int frame_idx, const lv_gpu_renderer_verify_stats_t * s)
{
    const int lite = verify_lite_mode();

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
        if(s->region_greenish_count < 1 && s->region_visible_count < (lite ? 2u : 8u)) {
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
        if(s->region_opaque_count < (lite ? 4u : 32u)) {
            printf("LVGL_VERIFY: FAIL scenario=%d frame=%d opaque=%u colorful=%u\n",
                   scenario_id, frame_idx,
                   (unsigned)s->region_opaque_count, (unsigned)s->region_colorful_count);
            return 0;
        }
        if(s->region_colorful_count < (lite ? 4u : 32u)) {
            printf("LVGL_VERIFY: FAIL scenario=%d frame=%d colorful=%u (expect colored tiles)\n",
                   scenario_id, frame_idx, (unsigned)s->region_colorful_count);
            return 0;
        }
        return 1;
    }

    if(scenario_id == 3) {
        const int expect = lvgl_scenario3_get_mesh_count();
        const uint32_t min_items = expect > 0 ? (uint32_t)(expect * 3 / 4) : 80u;
        if(s->last_flush_items < min_items) {
            printf("LVGL_VERIFY: FAIL scenario=%d frame=%d flush_items=%u (expect >=%u wireframes)\n",
                   scenario_id, frame_idx, (unsigned)s->last_flush_items, (unsigned)min_items);
            return 0;
        }
        if(s->region_max_alpha < 24 && s->flush_max_alpha < 24) {
            printf("LVGL_VERIFY: FAIL scenario=%d frame=%d max_alpha=%u flush_max=%u (expect visible wireframe)\n",
                   scenario_id, frame_idx, (unsigned)s->region_max_alpha, (unsigned)s->flush_max_alpha);
            return 0;
        }
        if(s->region_visible_count < (lite ? 2u : 8u)) {
            printf("LVGL_VERIFY: FAIL scenario=%d frame=%d visible=%u (expect wireframe pixels)\n",
                   scenario_id, frame_idx, (unsigned)s->region_visible_count);
            return 0;
        }
        return 1;
    }

    if(scenario_id == 4) {
        if(s->last_flush_items < 1) {
            printf("LVGL_VERIFY: FAIL scenario=%d frame=%d flush_items=%u (expect >=1 3D button)\n",
                   scenario_id, frame_idx, (unsigned)s->last_flush_items);
            return 0;
        }
        if(s->region_max_alpha < 32 && s->flush_max_alpha < 32) {
            printf("LVGL_VERIFY: FAIL scenario=%d frame=%d max_alpha=%u (expect visible 3D button)\n",
                   scenario_id, frame_idx, (unsigned)s->region_max_alpha);
            return 0;
        }
        if(s->region_bluish_count < (lite ? 1u : 4u)) {
            printf("LVGL_VERIFY: FAIL scenario=%d frame=%d bluish=%u (expect blue button pixels)\n",
                   scenario_id, frame_idx, (unsigned)s->region_bluish_count);
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

    lv_gpu_renderer_verify_stats_t stats;
    if(!lv_gpu_renderer_verify_stats(disp, &stats)) {
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

    if(frame_idx == 1 && getenv("LVGL_VERIFY_DUMP")) {
        const char * dump_dir = getenv("LVGL_VERIFY_DUMP");
        char frame_path[512];
        lv_snprintf(frame_path, sizeof(frame_path), "%s/frame_lvgl.rgba", dump_dir);
        if(lv_gpu_renderer_dump_frame_lvgl(disp, frame_path)) {
            printf("LVGL_VERIFY: dump frame -> %s\n", frame_path);
        }
#if LV_USE_SNAPSHOT
        if(scenario_id == 1) {
            char snap_path[512];
            lv_snprintf(snap_path, sizeof(snap_path), "%s/snap1_lvgl.rgba", dump_dir);
            if(lv_3d_plane_dump_snapshot_lvgl(1, snap_path)) {
                printf("LVGL_VERIFY: dump snap1 -> %s\n", snap_path);
            }
        }
#endif
    }

    if(stats.corner_min_alpha > 10 && scenario_id != 1 && scenario_id != 3 && scenario_id != 4) {
        /* Scenario 1/3: opaque viewport backdrop; 4: opaque UI chrome. */
        printf("LVGL_VERIFY: FAIL scenario=%d frame=%d corner_min_alpha=%u (expect ~0 AR passthrough)\n",
               scenario_id, frame_idx, (unsigned)stats.corner_min_alpha);
        verify_done = 1;
        exit(1);
    }

    if(!verify_frame_content(frame_idx, &stats)) {
        verify_done = 1;
        exit(1);
    }

    if(!verify_gpu_path(&stats)) {
        verify_done = 1;
        exit(1);
    }

    if(!verify_fg_framegraph(&stats)) {
        verify_done = 1;
        exit(1);
    }

#if LV_USE_3D_SEGMENT_POOL
    if(scenario_id == 2) {
        const float min_z = lvgl_scenario2_get_min_seg_z();
        if(frames_checked == 0) s2_min_z_first = min_z;
        s2_min_z_last = min_z;
    }
#endif

    frames_checked++;
    verify_log_path_stats(&stats);

    if(scenario_id == 4) {
        printf("LVGL_VERIFY: frame %d/%d ok corner_a=%u max_a=%u flush_items=%u "
               "bluish=%u center_rgba=%u,%u,%u,%u\n",
               frame_idx, frames_to_check,
               (unsigned)stats.corner_min_alpha,
               (unsigned)stats.region_max_alpha,
               (unsigned)stats.last_flush_items,
               (unsigned)stats.region_bluish_count,
               (unsigned)stats.center_rgba[0],
               (unsigned)stats.center_rgba[1],
               (unsigned)stats.center_rgba[2],
               (unsigned)stats.center_rgba[3]);
    }
    else {
        printf("LVGL_VERIFY: frame %d/%d ok corner_a=%u max_a=%u flush_max=%u flush_items=%u visible=%u\n",
               frame_idx, frames_to_check,
               (unsigned)stats.corner_min_alpha,
               (unsigned)stats.region_max_alpha,
               (unsigned)stats.flush_max_alpha,
               (unsigned)stats.last_flush_items,
               (unsigned)stats.region_visible_count);
    }

    if(frames_checked >= frames_to_check) {
#if LV_USE_3D_SEGMENT_POOL
        if(scenario_id == 2) {
            const uint32_t recycled = lvgl_scenario2_get_recycle_count();
            const float dz = s2_min_z_last - s2_min_z_first;
            if(recycled < 1 && dz < 80.0f) {
                printf("LVGL_VERIFY: FAIL scenario=2 parallax min_z %.1f -> %.1f (dz=%.1f) recycle=%u\n",
                       s2_min_z_first, s2_min_z_last, dz, (unsigned)recycled);
                verify_done = 1;
                exit(1);
            }
            printf("LVGL_VERIFY: scenario2 parallax ok dz=%.1f recycle=%u\n", dz, (unsigned)recycled);
        }
#endif
        printf("LVGL_VERIFY: PASS scenario=%d checked_frames=%d seen_frames=%d samples_per_frame=%u\n",
               scenario_id, frames_to_check, frames_seen, (unsigned)stats.region_samples);
        if(env_bool("LVGL_VERIFY_GPU_PATH", scenario_id == 2 ? 1 : 0)) {
            printf("LVGL_VERIFY: gpu_path PASS scenario=%d\n", scenario_id);
        }
        if(env_bool("LVGL_VERIFY_FG", 1)) {
            printf("LVGL_VERIFY: fg PASS scenario=%d pass=%u batch=%u mat=%u gl_finish=%u\n",
                   scenario_id,
                   (unsigned)stats.fg_pass_count,
                   (unsigned)stats.fg_batch_count,
                   (unsigned)stats.fg_material_batches,
                   (unsigned)stats.fg_gl_finish_count);
        }
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
    s2_min_z_first = 0.0f;
    s2_min_z_last = 0.0f;
    warmup_left = env_int("LVGL_VERIFY_WARMUP", 8);
    frames_to_check = env_int("LVGL_VERIFY_FRAMES", 60);
    if(frames_to_check < 1) frames_to_check = 1;

    printf("LVGL_VERIFY: start scenario=%d warmup=%d frames=%d lite=%d samples=%u\n",
           scenario_id, warmup_left, frames_to_check,
           verify_lite_mode(),
           (unsigned)(verify_lite_mode() ? (8u * 5u + 9u) : (48u * 27u + 9u)));

    lv_gpu_renderer_set_frame_ready_cb(verify_frame_cb);
    lv_timer_create(verify_timer_cb, 16, NULL);
    return 0;
}

#else

int lvgl_verify_run(int id)
{
    LV_UNUSED(id);
    printf("LVGL_VERIFY: SKIP (gpu_renderer not enabled)\n");
    return 0;
}

#endif
