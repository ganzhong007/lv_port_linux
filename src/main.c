/*******************************************************************
 *
 * main.c - LVGL simulator for GNU/Linux
 *
 * Based on the original file from the repository
 *
 * @note eventually this file won't contain a main function and will
 * become a library supporting all major operating systems
 *
 * To see how each driver is initialized check the
 * 'src/lib/display_backends' directory
 *
 * - Clean up
 * - Support for multiple backends at once
 *   2025 EDGEMTech Ltd.
 *
 * Author: EDGEMTech Ltd, Erik Tagirov (erik.tagirov@edgemtech.ch)
 *
 ******************************************************************/
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl/lvgl.h"
#include "src/simple_button.h"
#include "src/simple_button_style.h"
#include "src/simple_anim.h"
#include "src/evgpu_benchmark.h"
#include "demo_g3_showcase.h"
#if LV_USE_DEMO_STRESS || LV_USE_DEMO_BENCHMARK || LV_USE_DEMO_RENDER || LV_USE_DEMO_VECTOR_GRAPHIC || LV_USE_DEMO_GLTF || LV_USE_DEMO_3DVIEWPORT || LV_USE_DEMO_3DSCENE || LV_USE_DEMO_3DVIEW
#include "lvgl/demos/lv_demos.h"
#endif

#include "src/lib/driver_backends.h"
#include "src/lib/simulator_util.h"
#include "src/lib/simulator_settings.h"

/* Internal functions */
static void configure_simulator(int argc, char ** argv);
static void print_lvgl_version(void);
static void print_usage(void);

/* contains the name of the selected backend if user
 * has specified one on the command line */
static char * selected_backend;

/* Global simulator settings, defined in lv_linux_backend.c */
extern simulator_settings_t settings;

#if LV_USE_SNAPSHOT
/**
 * One-shot screen dump via snapshot (SW / CPU buffer backends).
 * Set LVGL_BUF_DUMP=/path/out.ppm — fires after a few refresh ticks.
 */
static void buf_dump_timer_cb(lv_timer_t * t)
{
    static int ticks;
    const char * path = getenv("LVGL_BUF_DUMP");
    if(path == NULL) {
        lv_timer_delete(t);
        return;
    }

    lv_obj_invalidate(lv_screen_active());
    ticks++;
    if(ticks < 8) return;

    lv_draw_buf_t * snap = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_ARGB8888);
    if(snap == NULL || snap->data == NULL) {
        fprintf(stderr, "LVGL_BUF_DUMP: snapshot failed\n");
        lv_timer_delete(t);
        return;
    }

    const uint32_t w = snap->header.w;
    const uint32_t h = snap->header.h;
    const uint32_t stride = snap->header.stride;
    FILE * f = fopen(path, "wb");
    if(f) {
        fprintf(f, "P6\n%u %u\n255\n", w, h);
        for(uint32_t y = 0; y < h; y++) {
            const uint8_t * row = snap->data + (size_t)y * stride;
            for(uint32_t x = 0; x < w; x++) {
                /* ARGB8888 stored as B,G,R,A on little-endian LV_COLOR_FORMAT_ARGB8888 */
                fputc(row[x * 4 + 2], f); /* R */
                fputc(row[x * 4 + 1], f); /* G */
                fputc(row[x * 4 + 0], f); /* B */
            }
        }
        fclose(f);
        fprintf(stderr, "LVGL_BUF_DUMP wrote %s (%ux%u)\n", path, w, h);
    }
    lv_draw_buf_destroy(snap);
    lv_timer_delete(t);
}
#endif

/** Keep the screen dirty so LVGL_GL_DUMP (C_R_T/EVGPU) can count end_frames. */
static void gl_dump_kick_timer_cb(lv_timer_t * t)
{
    LV_UNUSED(t);
    if(getenv("LVGL_GL_DUMP") == NULL) return;
    lv_obj_invalidate(lv_screen_active());
}


/**
 * @brief Print LVGL version
 */
static void print_lvgl_version(void)
{
    fprintf(stdout, "%d.%d.%d-%s\n",
            LVGL_VERSION_MAJOR,
            LVGL_VERSION_MINOR,
            LVGL_VERSION_PATCH,
            LVGL_VERSION_INFO);
}

/**
 * @brief Print usage information
 */
static void print_usage(void)
{
    fprintf(stdout,
            "\nlvglsim [-V] [-B] [-f] [-m] [-b backend_name] [-W window_width] [-H window_height] [-R rotation]\n\n");
    fprintf(stdout, "-V print LVGL version\n");
    fprintf(stdout, "-B list supported backends\n");
    fprintf(stdout, "-f fullscreen\n");
    fprintf(stdout, "-m maximize\n");
}

/**
 * @brief Configure simulator
 * @description process arguments received by the program to select
 * appropriate options
 * @param argc the count of arguments in argv
 * @param argv The arguments
 */
static void configure_simulator(int argc, char ** argv)
{
    int opt = 0;

    selected_backend = NULL;
    driver_backends_register();

    const char * env_w = getenv("LV_SIM_WINDOW_WIDTH");
    const char * env_h = getenv("LV_SIM_WINDOW_HEIGHT");
    /* Default values */
    settings.window_width = atoi(env_w ? env_w : "800");
    settings.window_height = atoi(env_h ? env_h : "480");

    /* Parse the command-line options. */
    while((opt = getopt(argc, argv, "b:fmW:H:R:BVh")) != -1) {
        switch(opt) {
            case 'h':
                print_usage();
                exit(EXIT_SUCCESS);
                break;
            case 'V':
                print_lvgl_version();
                exit(EXIT_SUCCESS);
                break;
            case 'B':
                driver_backends_print_supported();
                exit(EXIT_SUCCESS);
                break;
            case 'b':
                if(driver_backends_is_supported(optarg) == 0) {
                    die("error no such backend: %s\n", optarg);
                }
                selected_backend = strdup(optarg);
                break;
            case 'f':
                settings.fullscreen = true;
                break;
            case 'm':
                settings.maximize = true;
                break;
            case 'W':
                settings.window_width = atoi(optarg);
                break;
            case 'H':
                settings.window_height = atoi(optarg);
                break;
            case 'R':
                switch(atoi(optarg)) {
                    case 0:
                        settings.rotation = LV_DISPLAY_ROTATION_0;
                        break;
                    case 90:
                        settings.rotation = LV_DISPLAY_ROTATION_90;
                        break;
                    case 180:
                        settings.rotation = LV_DISPLAY_ROTATION_180;
                        break;
                    case 270:
                        settings.rotation = LV_DISPLAY_ROTATION_270;
                        break;
                    default:
                        LV_LOG_WARN("Invalid rotation angle. Valid angles are {0, 90, 180, 270}");
                        break;
                }
                break;
            case ':':
                print_usage();
                die("Option -%c requires an argument.\n", optopt);
                break;
            case '?':
                print_usage();
                die("Unknown option -%c.\n", optopt);
        }
    }
}

/**
 * @brief entry point
 * @description start a demo
 * @param argc the count of arguments in argv
 * @param argv The arguments
 */
int main(int argc, char ** argv)
{

    configure_simulator(argc, argv);

    /* Initialize LVGL. */
    lv_init();

    /* Initialize the configured backend */
    if(driver_backends_init_backend(selected_backend) == -1) {
        die("Failed to initialize display backend");
    }
    if(settings.rotation) {
#if LV_USE_DRAW_NANOVG && LV_DRAW_TRANSFORM_USE_MATRIX
        lv_display_set_matrix_rotation(NULL, true);
#endif
        lv_display_set_rotation(NULL, settings.rotation);
    }

    /* Enable for EVDEV support */
#if LV_USE_EVDEV
    if(driver_backends_init_backend("EVDEV") == -1) {
        die("Failed to initialize evdev");
    }
#endif

    /* Minimal examples / demos — selected by -DLVGL_APP_DEMO=... */
#if defined(LVGL_APP_DEMO_STRESS) && LV_USE_DEMO_STRESS
    lv_demo_stress();
#elif defined(LVGL_APP_DEMO_SIMPLE_BUTTON_STYLE)
    simple_button_style_create();
#elif defined(LVGL_APP_DEMO_SIMPLE_ANIM)
    simple_anim_create();
#elif defined(LVGL_APP_DEMO_BENCHMARK) && LV_USE_DEMO_BENCHMARK
    lv_demo_benchmark();
#elif defined(LVGL_APP_DEMO_RENDER) && LV_USE_DEMO_RENDER
    lv_demo_render(LV_DEMO_RENDER_SCENE_FILL, LV_OPA_COVER);
#elif defined(LVGL_APP_DEMO_VECTOR_GRAPHIC) && LV_USE_DEMO_VECTOR_GRAPHIC
    lv_demo_vector_graphic_not_buffered();
#elif defined(LVGL_APP_DEMO_GLTF) && LV_USE_DEMO_GLTF
    const char * gltf_path = getenv_default("LVGL_GLTF_MODEL",
                                            "A:lvgl/examples/libs/gltf/lvgl_logo.glb");
    lv_demo_gltf(gltf_path);
#elif defined(LVGL_APP_DEMO_3DVIEWPORT) && LV_USE_DEMO_3DVIEWPORT
    lv_demo_3dviewport();
#elif defined(LVGL_APP_DEMO_3DSCENE) && LV_USE_DEMO_3DSCENE
    lv_demo_3dscene();
#elif defined(LVGL_APP_DEMO_3DVIEW) && LV_USE_DEMO_3DVIEW
    lv_demo_3dview();
#elif defined(LVGL_APP_DEMO_EVGPU_BENCH)
    evgpu_benchmark_create();
#elif defined(LVGL_APP_DEMO_G3_SHOWCASE)
    demo_g3_showcase_init();
#else
    simple_button_create();
#endif

    /* Optional SW/CPU buffer dump for backend compares: LVGL_BUF_DUMP=/tmp/out.ppm */
#if LV_USE_SNAPSHOT
    if(getenv("LVGL_BUF_DUMP")) {
        lv_timer_create(buf_dump_timer_cb, 100, NULL);
    }
#endif
    if(getenv("LVGL_GL_DUMP")) {
        lv_timer_create(gl_dump_kick_timer_cb, 50, NULL);
    }

    /* Enter the run loop of the selected backend */
    driver_backends_run_loop();

    return 0;
}
