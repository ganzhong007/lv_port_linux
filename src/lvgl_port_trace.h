/**
 * @file lvgl_port_trace.h
 * @brief Layer-tagged trace logs for the simple button walkthrough.
 */
#ifndef LVGL_PORT_TRACE_H
#define LVGL_PORT_TRACE_H

#include <stdio.h>

#ifndef LV_USE_PORT_LAYER_TRACE
#define LV_USE_PORT_LAYER_TRACE 0
#endif

#if LV_USE_PORT_LAYER_TRACE
#define LVGL_PORT_TRACE(layer, fmt, ...) \
    do { \
        fprintf(stderr, "[LVGL:%s] " fmt "\n", layer, ##__VA_ARGS__); \
        fflush(stderr); \
    } while(0)
#else
#define LVGL_PORT_TRACE(layer, fmt, ...) ((void)0)
#endif

#endif /*LVGL_PORT_TRACE_H*/
