/**
 * @file simple_button_shot.h
 * @brief Save active screen to PNG (requires LV_USE_SNAPSHOT).
 */
#ifndef SIMPLE_BUTTON_SHOT_H
#define SIMPLE_BUTTON_SHOT_H

#ifdef __cplusplus
extern "C" {
#endif

/** Force refresh and write the active screen to a PNG file. Returns 0 on success. */
int simple_button_save_screen_png(const char * path);

#ifdef __cplusplus
}
#endif

#endif /*SIMPLE_BUTTON_SHOT_H*/
