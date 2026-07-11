/**
 * @file simple_button_shot.c
 * @brief LVGL snapshot -> PPM (convert to PNG with ImageMagick if needed).
 */
#include "lvgl/lvgl.h"
#include "simple_button_shot.h"
#include <stdio.h>

int simple_button_save_screen_png(const char * path)
{
#if !LV_USE_SNAPSHOT
    LV_UNUSED(path);
    return -1;
#else
    lv_refr_now(NULL);

    lv_draw_buf_t * buf = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_ARGB8888);
    if(buf == NULL || buf->data == NULL) {
        return -1;
    }

    const int32_t w = buf->header.w;
    const int32_t h = buf->header.h;
    const uint32_t stride = buf->header.stride;

    FILE * f = fopen(path, "wb");
    if(f == NULL) {
        lv_draw_buf_destroy(buf);
        return -1;
    }

    fprintf(f, "P6\n%d %d\n255\n", (int)w, (int)h);

    for(int32_t y = 0; y < h; y++) {
        const lv_color32_t * row = (const lv_color32_t *)(buf->data + y * stride);
        for(int32_t x = 0; x < w; x++) {
            const unsigned char rgb[3] = { row[x].red, row[x].green, row[x].blue };
            if(fwrite(rgb, 1, 3, f) != 3) {
                fclose(f);
                lv_draw_buf_destroy(buf);
                return -1;
            }
        }
    }

    fclose(f);
    lv_draw_buf_destroy(buf);
    return 0;
#endif
}
