/**
 * @file lvgl_demos.h
 */

#ifndef LVGL_DEMOS_H
#define LVGL_DEMOS_H

#ifdef __cplusplus
extern "C" {
#endif

void lvgl_scenario1_launcher_create(void);
void lvgl_scenario2_skyline_create(void);

/** Segment pool recycle count (scenario 2 Phase 2 verify). */
uint32_t lvgl_scenario2_get_recycle_count(void);
/** Minimum segment base z (scenario 2 parallax verify). */
float lvgl_scenario2_get_min_seg_z(void);

/** Run automated verification; returns 0 on pass. */
int lvgl_verify_run(int scenario_id);

#ifdef __cplusplus
}
#endif

#endif /*LVGL_DEMOS_H*/
