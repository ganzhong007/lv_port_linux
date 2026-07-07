/**
 * @file lvgl_demos.h
 */

#ifndef LVGL_DEMOS_H
#define LVGL_DEMOS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Advance animated demos (segment pool scroll, etc.) before display refresh. */
void lvgl_demos_pre_refresh(uint32_t elapsed_ms);

/** Skip lv_refr_now once viewports are recorded (pure 3D animation, e.g. scenario 5). */
bool lvgl_demos_direct_gpu_present(void);
void lvgl_demos_set_direct_gpu_present(bool enable);
/** True when direct present is active and the first viewport pass was recorded. */
bool lvgl_demos_skip_lv_refresh(void);
/** Mark one GPU present for direct-render demos (animation timer). */
void lvgl_demos_request_gpu_frame(void);
/** True once after request_gpu_frame; clears the pending flag. */
bool lvgl_demos_consume_gpu_frame(void);
/** True while a direct-render frame is waiting for flip/present. */
bool lvgl_demos_gpu_frame_pending(void);

void lvgl_scenario1_launcher_create(void);
void lvgl_scenario2_skyline_create(void);
void lvgl_scenario3_gpu_stress_create(void);
void lvgl_scenario4_3dbutton_create(void);
void lvgl_scenario5_stereo_cube_create(void);
void lvgl_scenario6_stereo_sphere_create(void);
void lvgl_scenario7_video_crop_create(void);
/** Elapsed animation time in ms (scenario 7 verify). */
uint32_t lvgl_scenario7_get_anim_ms(void);

/** Active wireframe mesh count (scenario 3 verify). */
int lvgl_scenario3_get_mesh_count(void);

/** Segment pool recycle count (scenario 2 Phase 2 verify). */
uint32_t lvgl_scenario2_get_recycle_count(void);
/** Minimum segment base z (scenario 2 parallax verify). */
float lvgl_scenario2_get_min_seg_z(void);

/** Current cube yaw in degrees (scenario 5 verify). */
float lvgl_scenario5_get_yaw_deg(void);
/** Current sphere yaw in degrees (scenario 6 verify). */
float lvgl_scenario6_get_yaw_deg(void);

/** Run automated verification; returns 0 on pass. */
int lvgl_verify_run(int scenario_id);

#ifdef __cplusplus
}
#endif

#endif /*LVGL_DEMOS_H*/
