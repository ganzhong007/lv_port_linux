/**
 * @file main.c — lv_port_linux simulator with LVGL scenario demos
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl/lvgl.h"
#include "src/lib/driver_backends.h"
#include "src/lib/simulator_util.h"
#include "src/lib/simulator_settings.h"
#include "src/demos/lvgl_demos.h"

static char * selected_backend;
extern simulator_settings_t settings;

static void configure_simulator(int argc, char ** argv)
{
    int opt = 0;
    selected_backend = NULL;
    driver_backends_register();

    const char * env_w = getenv("LV_SIM_WINDOW_WIDTH");
    const char * env_h = getenv("LV_SIM_WINDOW_HEIGHT");
    settings.window_width = atoi(env_w ? env_w : "800");
    settings.window_height = atoi(env_h ? env_h : "480");

    while((opt = getopt(argc, argv, "b:fmW:H:R:BVh")) != -1) {
        switch(opt) {
            case 'h':
                fprintf(stdout, "lvglsim [-V] [-B] [-b backend] [-W w] [-H h]\n");
                exit(EXIT_SUCCESS);
            case 'V':
                fprintf(stdout, "%d.%d.%d-%s\n", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
                        LVGL_VERSION_PATCH, LVGL_VERSION_INFO);
                exit(EXIT_SUCCESS);
            case 'B':
                driver_backends_print_supported();
                exit(EXIT_SUCCESS);
            case 'b':
                if(driver_backends_is_supported(optarg) == 0) die("error no such backend: %s\n", optarg);
                selected_backend = strdup(optarg);
                break;
            case 'f': settings.fullscreen = true; break;
            case 'm': settings.maximize = true; break;
            case 'W': settings.window_width = atoi(optarg); break;
            case 'H': settings.window_height = atoi(optarg); break;
            case 'R':
                switch(atoi(optarg)) {
                    case 90: settings.rotation = LV_DISPLAY_ROTATION_90; break;
                    case 180: settings.rotation = LV_DISPLAY_ROTATION_180; break;
                    case 270: settings.rotation = LV_DISPLAY_ROTATION_270; break;
                    default: settings.rotation = LV_DISPLAY_ROTATION_0; break;
                }
                break;
            default:
                break;
        }
    }
}

int main(int argc, char ** argv)
{
    configure_simulator(argc, argv);
    lv_init();

    if(driver_backends_init_backend(selected_backend) == -1) {
        die("Failed to initialize display backend");
    }

    if(settings.rotation) {
        lv_display_set_rotation(NULL, settings.rotation);
    }

    const char * scen = getenv("LVGL_SCENARIO");
    int scenario = scen ? atoi(scen) : 2;
    const char * verify = getenv("LVGL_VERIFY");

#if LV_USE_3D
    if(scenario == 1) {
        lvgl_scenario1_launcher_create();
    }
    else if(scenario == 3) {
        lvgl_scenario3_gpu_stress_create();
    }
    else {
        lvgl_scenario2_skyline_create();
    }
#else
    lv_demo_widgets();
#endif

    if(verify && verify[0] == '1') {
        lvgl_verify_run(scenario);
    }
    else {
        lv_obj_invalidate(lv_screen_active());
    }

    driver_backends_run_loop();
    return 0;
}
