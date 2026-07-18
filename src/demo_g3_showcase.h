/*
 * Copyright (c) 2025 Even Technology
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file demo_g3_showcase.h
 * @brief Focused showcase of the G3 Glasses OS LVGL techniques.
 *
 * A single self-contained screen that demonstrates, side by side, the five
 * building blocks documented in
 * `Documents/G3_Glasses_OS_HTML对应LVGL实现讲解.md`:
 *   1. Player animation  - equalizer bars + breathing "live" dot
 *   2. World gradient     - radial backgrounnette
 *   3. Mouse hover        - LV_STATE_HOVERED highlight with transition
 *   4. Boot animation     - replayable progress bar + status text + fade out
 *   5. Layout             - the showcase itself is a Grid of Flex cards
 */

#ifndef DEMO_G3_SHOWCASE_H_
#define DEMO_G3_SHOWCASE_H_

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Build the G3 technique showcase on the active screen and start its timers. */
void demo_g3_showcase_init(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* DEMO_G3_SHOWCASE_H_ */
