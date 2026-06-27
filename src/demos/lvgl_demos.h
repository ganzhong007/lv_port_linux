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

/** Run automated verification; returns 0 on pass. */
int lvgl_verify_run(int scenario_id);

#ifdef __cplusplus
}
#endif

#endif /*LVGL_DEMOS_H*/
