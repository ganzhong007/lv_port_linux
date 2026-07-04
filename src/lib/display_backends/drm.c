/**
 *
 * @file drm.c
 *
 * The DRM/KMS backend
 *
 * Based on the original file from the repository
 *
 * - Move to a separate file
 *   2025 EDGEMTech Ltd.
 *
 * Author: EDGEMTech Ltd, Erik Tagirov (erik.tagirov@edgemtech.ch)
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lvgl/lvgl.h"
#if LV_USE_LINUX_DRM
#include "../simulator_util.h"
#include "../simulator_settings.h"
#include "../backends.h"
#if LV_USE_DRAW_GPU_RENDERER
#include "../../demos/lvgl_demos.h"

extern bool lv_linux_drm_gpu_flip_ready(lv_display_t * disp);
extern bool lv_linux_drm_gpu_present_ex(lv_display_t * disp);
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void run_loop_drm(void);
static lv_display_t * init_drm(void);


/**********************
 *  STATIC VARIABLES
 **********************/
static char * backend_name = "DRM";

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Register the backend
 *
 * @param backend the backend descriptor
 * @description configure the descriptor
 */
int backend_init_drm(backend_t * backend)
{
    LV_ASSERT_NULL(backend);

    backend->handle->display = malloc(sizeof(display_backend_t));
    LV_ASSERT_NULL(backend->handle->display);

    backend->handle->display->init_display = init_drm;
    backend->handle->display->run_loop = run_loop_drm;
    backend->name = backend_name;
    backend->type = BACKEND_DISPLAY;

    return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Initialize the DRM display driver
 *
 * @return the LVGL display
 */
static lv_display_t * init_drm(void)
{
    const char * device = getenv_default("LV_LINUX_DRM_CARD", lv_linux_drm_find_device_path());
    lv_display_t * disp = lv_linux_drm_create();

    if(disp == NULL) {
        return NULL;
    }

    lv_linux_drm_set_file(disp, device, -1);

    return disp;
}


/**
 * The run loop of the DRM driver
 */
static void run_loop_drm(void)
{
    uint32_t idle_time;

    while(true) {
        idle_time = lv_timer_handler();
#if LV_USE_DRAW_GPU_RENDERER
        {
            lv_display_t * disp = lv_display_get_default();
            if(disp) {
                if(!lvgl_demos_skip_lv_refresh()) {
                    lv_refr_now(disp);
                    lv_linux_drm_gpu_present(disp);
                }
                else if(lvgl_demos_gpu_frame_pending()) {
                    if(lv_linux_drm_gpu_flip_ready(disp) && lv_linux_drm_gpu_present_ex(disp)) {
                        lvgl_demos_consume_gpu_frame();
                    }
                    else if(!lv_linux_drm_gpu_flip_ready(disp)) {
                        usleep(500);
                    }
                }
            }
        }
#endif
        if(idle_time > 0 && !lvgl_demos_skip_lv_refresh()) {
            usleep(idle_time * 1000);
        }
    }
}

#endif /*#if LV_USE_LINUX_DRM*/
