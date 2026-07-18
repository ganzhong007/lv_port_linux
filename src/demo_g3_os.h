/*
 * Copyright (c) 2025 Even Technology
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file demo_g3_os.h
 * @brief LVGL recreation of the "G3 Glasses OS" spatial-UX prototype.
 *
 * Monochrome phosphor-green micro-LED HUD: a boot sequence, then the
 * "Base Flow" OS -- a horizontal carousel of live glass tiles
 * (Control / Dashboard / Home Desk 1 / Home Desk 2), a top notifications
 * overlay, a bottom apps overlay, and tap-to-expand tiles.
 *
 * Ported from the single-file HTML/CSS/JS demo (g3-glasses-os-demo.html).
 */

#ifndef DEMO_G3_OS_H_
#define DEMO_G3_OS_H_

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Build the G3 Glasses OS demo on the active screen and start its timers. */
void demo_g3_os_init(void);

/**
 * Feed a mouse-wheel / trackpad scroll step into the demo.
 * @param dx  horizontal step: -1, 0 or +1
 * @param dy  vertical step:   -1, 0 or +1 (+1 = scroll up/away)
 * @note  Matches the signature of `lv_glfw_scroll_handler_t`, so it can be
 *        registered directly via `lv_opengles_glfw_set_scroll_handler()`.
 */
void demo_g3_os_wheel(int32_t dx, int32_t dy);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* DEMO_G3_OS_H_ */
