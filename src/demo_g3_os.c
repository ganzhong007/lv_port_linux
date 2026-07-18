/*
 * Copyright (c) 2025 Even Technology
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file demo_g3_os.c
 * @brief LVGL recreation of the "G3 Glasses OS" spatial-UX prototype.
 *
 * Ported from the single-file HTML demo. Phosphor-green micro-LED HUD:
 * boot sequence -> Base Flow OS carousel (Control / Dashboard / Desk1 /
 * Desk2) with live glass tiles, top notifications overlay, bottom apps
 * overlay, pager dots, status bar and tap-to-expand tiles.
 */

#include "demo_g3_os.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* ================= Look & feel ================= */
#define G_GREEN         0x45FF8Au   /* phosphor-mint HUD green */
#define G_RREEN         0xFF0000u   /* phosphor-mint HUD green */
#define OP_BRIGHT       255
#define OP_DIM          120
#define OP_FAINT        60

#define SCREEN_W        1200
#define SCREEN_H        720
#define SCREEN_COUNT    4
#define START_INDEX     1           /* dashboard = home/center */

#define PAD_TOP         100
#define PAD_BOTTOM      96
#define PAD_SIDE        120         /* wide frame so the "world" shows around the HUD */
#define TILE_GAP        21

/* Fonts available in this build: montserrat 14/20/22/24/26/30/36/40. */
#define FONT_SM         (&lv_font_montserrat_22)
#define FONT_MD         (&lv_font_montserrat_30)
#define FONT_LG         (&lv_font_montserrat_36)
#define FONT_XL         (&lv_font_montserrat_40)

/* App identifiers carried on each live tile (used when expanding). */
typedef enum {
    APP_NONE = 0,
    APP_MUSIC,
    APP_NAVIGATE,
    APP_AGENT,
    APP_ACTIVITY,
    APP_MESSAGES,
    APP_TRANSLATE,
    APP_CALENDAR,
    APP_CLOCK,
    APP_WEATHER,
} g3_app_t;

/* ================= Live content model ================= */
typedef struct {
    const char * title;
    const char * artist;
    int32_t      dur;
} track_t;

static const track_t TRACKS[] = {
    { "Midnight City", "M83",      243 },
    { "Nightcall",     "Kavinsky", 258 },
    { "Resonance",     "HOME",     213 },
};
#define TRACK_COUNT ((int32_t)(sizeof(TRACKS) / sizeof(TRACKS[0])))

static const char * const TRANSLATE_SRC[] = {
    "\"Where is the station?\"",
    "\"Good morning!\"",
    "\"How much is this?\"",
    "\"See you tomorrow.\"",
};
/* Real Chinese (used when the CJK font loads) - common glyphs. */
static const char * const TRANSLATE_DST_CJK[] = {
    "\xE8\xBD\xA6\xE7\xAB\x99\xE5\x9C\xA8\xE5\x93\xAA\xE9\x87\x8C\xEF\xBC\x9F", /* 车站在哪里？ */
    "\xE6\x97\xA9\xE4\xB8\x8A\xE5\xA5\xBD\xEF\xBC\x81",                         /* 早上好！ */
    "\xE8\xBF\x99\xE4\xB8\xAA\xE5\xA4\x9A\xE5\xB0\x91\xE9\x92\xB1\xEF\xBC\x9F", /* 这个多少钱？ */
    "\xE6\x98\x8E\xE5\xA4\xA9\xE8\xA7\x81\xE3\x80\x82",                         /* 明天见。 */
};
/* ASCII pinyin fallback (used if the CJK font fails to load). */
static const char * const TRANSLATE_DST_PY[] = {
    "che zhan zai na li?",
    "zao shang hao!",
    "zhe ge duo shao qian?",
    "ming tian jian.",
};
#define TRANSLATE_COUNT ((int32_t)(sizeof(TRANSLATE_SRC) / sizeof(TRANSLATE_SRC[0])))

typedef struct {
    const char * ico;
    const char * from;
    const char * time;
    const char * text;
    bool         summary;
} notif_t;

static const notif_t NOTIFS[] = {
    { LV_SYMBOL_EYE_OPEN, "AI Summary", "now",
      "6 notifications: 2 messages, a reminder in 22 min, 3 low-priority. Nothing urgent.", true },
    { LV_SYMBOL_ENVELOPE, "Messages - Alex", "2m",  "ship it - the build looks great", false },
    { LV_SYMBOL_LIST,     "Calendar",       "8m",  "Design Review starts in 22 minutes - Room 4", false },
    { LV_SYMBOL_AUDIO,    "Music",          "15m", "New release from M83 is available", false },
    { LV_SYMBOL_GPS,      "Navigate",       "31m", "Traffic is light - 14 min to home", false },
    { LV_SYMBOL_BELL,     "Weather",        "1h",  "Clear skies all afternoon, high of 24", false },
};
#define NOTIF_COUNT ((int32_t)(sizeof(NOTIFS) / sizeof(NOTIFS[0])))

typedef struct {
    const char * ico;
    const char * name;
} appentry_t;

static const appentry_t APPS[] = {
    { LV_SYMBOL_GPS,      "Navigate" },   { LV_SYMBOL_LIST,     "Translate" },
    { LV_SYMBOL_FILE,     "Prompter" },   { LV_SYMBOL_EDIT,     "QuickNote" },
    { LV_SYMBOL_AUDIO,    "Music" },      { LV_SYMBOL_BELL,     "Weather" },
    { LV_SYMBOL_REFRESH,  "Timer" },      { LV_SYMBOL_SETTINGS, "Settings" },
};
#define APP_COUNT ((int32_t)(sizeof(APPS) / sizeof(APPS[0])))

/* Control-panel toggles / sliders */
#define CTRL_COUNT 8

/* ================= State ================= */
typedef struct {
    lv_obj_t * carousel;   /* clip window */
    lv_obj_t * track;      /* holds the 4 screens, animated on x */
    lv_obj_t * ov_notify;
    lv_obj_t * ov_apps;
    lv_obj_t * expanded_layer;
    lv_obj_t * expanded;    /* current expanded tile or NULL */
    lv_obj_t * boot;

    lv_obj_t * sb_left;
    lv_obj_t * clock_lbl;
    lv_obj_t * batt_lbl;
    lv_obj_t * pager_dot[SCREEN_COUNT];
    /* live tile widgets */
    lv_obj_t * m_title;
    lv_obj_t * m_artist;
    lv_obj_t * m_time;
    lv_obj_t * m_bar;
    lv_obj_t * a_steps;
    lv_obj_t * a_goal;
    lv_obj_t * a_bar;
    lv_obj_t * n_eta;
    lv_obj_t * n_dist;
    lv_obj_t * n_instr;
    lv_obj_t * ag_status;
    lv_obj_t * msg_from;
    lv_obj_t * msg_text;
    lv_obj_t * msg_meta;
    lv_obj_t * tr_src;
    lv_obj_t * tr_dst;
    lv_obj_t * cal_when;
    lv_obj_t * clk_big;
    lv_obj_t * clk_date;

    lv_obj_t * live_dot[8];
    int32_t    dot_count;

    /* live data */
    int32_t music_idx;
    int32_t music_t;
    int32_t steps;
    int32_t goal;
    int32_t nav_eta;      /* seconds  */
    int32_t nav_dist_x10; /* miles*10 */
    int32_t nav_instr;
    int32_t cal_mins;
    int32_t trans_i;
    int32_t msg_unread;
    int32_t msg_age;
    uint32_t sec;

    int32_t  index;
    int32_t  overlay;     /* 0 none, 1 notify, 2 apps */
    bool     dot_on;

    lv_timer_t * live_timer;
    lv_timer_t * boot_timer;
    int32_t      boot_step;
    lv_obj_t *   boot_status;

    lv_font_t *  font_cjk;      /* runtime-loaded SimSun CJK font, or NULL */

    /* pointer drag (mouse / touch) */
    int32_t      drag_start_x;
    int32_t      drag_start_y;
    int32_t      track_start_x;
    bool         drag_moved;
    bool         dragging_h;
} g3_ctx_t;

static g3_ctx_t g3;

static const char * const NAV_INSTRS[] = {
    "Turn left on 5th St",
    "Continue for 1.2 mi",
    "Slight right onto Market St",
    "Arriving - home on the right",
};

/* ================= Chat model (AI Agent + Messages) ================= */
#define CHAT_MAX       24
#define CHAT_TEXT_MAX  160

typedef struct {
    bool me;                    /* true: right-aligned "me" bubble, false: "them" */
    char text[CHAT_TEXT_MAX];
} chat_msg_t;

/* Persistent per-app history (survives expand/collapse). */
static chat_msg_t agent_hist[CHAT_MAX];
static int32_t    agent_n;
static chat_msg_t msgs_hist[CHAT_MAX];
static int32_t    msgs_n;

/* Canned "Alex" replies for the Messages chat (cycled). */
static const char * const ALEX_REPLIES[] = {
    "sounds good", "on it", "haha nice", "let's do it",
    "thanks!", "see you at standup", "perfect",
};
#define ALEX_COUNT ((int32_t)(sizeof(ALEX_REPLIES) / sizeof(ALEX_REPLIES[0])))
static int32_t alex_idx;

/* Quick-reply chips shown under each chat (no keyboard needed in the sim). */
static const char * const AGENT_CHIPS[] = { "Draft a reply", "Today's summary", "Thanks!" };
static const char * const MSGS_CHIPS[]  = { "On my way", "Sounds good", "Ping me later" };
#define CHIP_COUNT 3

/* Runtime chat widgets (valid only while a chat view is expanded). */
static g3_app_t    chat_open_app;
static lv_obj_t *  chat_log;
static lv_timer_t * chat_reply_timer;
/* ================= Small helpers ================= */
/* Animation helpers defined further below but used by the tile builders. */
static void start_breathing(lv_obj_t * dot);
static void anim_bar_h_cb(void * obj, int32_t v);

static lv_obj_t * g3_label(lv_obj_t * parent, const char * txt,
                           const lv_font_t * font, lv_opa_t opa)
{
    lv_obj_t * l = lv_label_create(parent);

    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_text_opa(l, opa, 0);
    lv_obj_set_style_text_font(l, font, 0);
    return l;
}

/* Transparent, borderless, non-scrollable container. */
static lv_obj_t * g3_plain(lv_obj_t * parent)
{
    lv_obj_t * o = lv_obj_create(parent);

    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_EVENT_BUBBLE);
    return o;
}

/* A horizontal head row: dim title on the left, icon on the right.
 * If live is true a pulsing "live dot" precedes the title. */
static lv_obj_t * g3_head(lv_obj_t * tile, const char * title, const char * icon, bool live)
{
    lv_obj_t * row = g3_plain(tile);

    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t * left = g3_plain(row);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(left, 7, 0);

    if (live && g3.dot_count < 8) {
        lv_obj_t * dot = lv_obj_create(left);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(G_GREEN), 0);
        lv_obj_set_style_bg_opa(dot, OP_BRIGHT, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        start_breathing(dot);
        g3.live_dot[g3.dot_count++] = dot;
    }
    g3_label(left, title, FONT_SM, OP_DIM);

    if (icon != NULL) {
        g3_label(row, icon, FONT_SM, OP_BRIGHT);
    }
    return row;
}

/* Five phosphor equalizer bars that bob up and down (HTML .eq / eqbar). Each
 * bar animates its own height on a staggered timeline so the group "dances". */
static void g3_eq(lv_obj_t * parent)
{
    static const int32_t eq_dur[5]   = { 900, 750, 1050, 850, 900 };
    static const int32_t eq_delay[5] = { 0, 180, 360, 100, 280 };
    lv_obj_t * eq = g3_plain(parent);
    int32_t    i;

    lv_obj_set_size(eq, LV_SIZE_CONTENT, 24);
    lv_obj_set_flex_flow(eq, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(eq, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(eq, 3, 0);

    for (i = 0; i < 5; i++) {
        lv_anim_t  a;
        lv_obj_t * bar = lv_obj_create(eq);

        lv_obj_set_width(bar, 4);
        lv_obj_set_height(bar, 6);
        lv_obj_set_style_radius(bar, 2, 0);
        lv_obj_set_style_bg_color(bar, lv_color_hex(G_GREEN), 0);
        lv_obj_set_style_bg_opa(bar, OP_BRIGHT, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_pad_all(bar, 0, 0);
        lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

        lv_anim_init(&a);
        lv_anim_set_var(&a, bar);
        lv_anim_set_exec_cb(&a, anim_bar_h_cb);
        lv_anim_set_values(&a, 6, 24);
        lv_anim_set_duration(&a, eq_dur[i] / 2);
        lv_anim_set_playback_duration(&a, eq_dur[i] / 2);
        lv_anim_set_delay(&a, eq_delay[i]);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_start(&a);
    }
}

/* Flexible spacer that pushes following content to the bottom. */
static void g3_grow(lv_obj_t * parent)
{
    lv_obj_t * s = g3_plain(parent);
    lv_obj_set_width(s, lv_pct(100));
    lv_obj_set_flex_grow(s, 1);
}

/* Progress bar; returns the inner fill object (set its width via lv_pct). */
static lv_obj_t * g3_bar(lv_obj_t * parent, int32_t pct)
{
    lv_obj_t * trk = lv_obj_create(parent);

    lv_obj_set_width(trk, lv_pct(100));
    lv_obj_set_height(trk, 6);
    lv_obj_set_style_radius(trk, 3, 0);
    lv_obj_set_style_bg_color(trk, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_bg_opa(trk, 40, 0);
    lv_obj_set_style_border_width(trk, 0, 0);
    lv_obj_set_style_pad_all(trk, 0, 0);
    lv_obj_remove_flag(trk, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * fill = lv_obj_create(trk);
    lv_obj_set_height(fill, lv_pct(100));
    lv_obj_set_width(fill, lv_pct(pct < 0 ? 0 : (pct > 100 ? 100 : pct)));
    lv_obj_align(fill, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_radius(fill, 3, 0);
    lv_obj_set_style_bg_color(fill, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_bg_opa(fill, OP_BRIGHT, 0);
    lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_set_style_pad_all(fill, 0, 0);
    lv_obj_remove_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
    return fill;
}

/* A glass tile: rounded rect, faint green fill, green edge, column flex. */
static lv_obj_t * g3_tile(lv_obj_t * parent)
{
    lv_obj_t * t = lv_obj_create(parent);

    lv_obj_set_style_bg_color(t, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_bg_opa(t, 16, 0);
    lv_obj_set_style_border_color(t, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_border_opa(t, 130, 0);
    lv_obj_set_style_border_width(t, 1, 0);
    lv_obj_set_style_radius(t, 14, 0);
    lv_obj_set_style_pad_all(t, 14, 0);
    lv_obj_set_style_pad_row(t, 5, 0);
    lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(t, LV_OBJ_FLAG_EVENT_BUBBLE);
    return t;
}

/* ================= Hover highlight (mouse enter/leave) ================= */
static lv_style_transition_dsc_t g_hover_trans;

static void hover_trans_init(void)
{
    static const lv_style_prop_t props[] = {
        LV_STYLE_BORDER_OPA, LV_STYLE_BORDER_WIDTH, LV_STYLE_BG_OPA,
        LV_STYLE_SHADOW_WIDTH, LV_STYLE_SHADOW_OPA, LV_STYLE_TRANSLATE_Y,
        LV_STYLE_PROP_INV
    };
    lv_style_transition_dsc_init(&g_hover_trans, props, lv_anim_path_ease_out, 220, 0, NULL);
}

/* Objects created with lv_obj_create are clickable by default in this build, so
 * the decorative children inside a tile (rows, bars, spacers) would be returned
 * by the hit-test as the "deepest clickable" and steal hover/click from the
 * tile. Clear the flag on every descendant so the tile itself stays the target. */
static void clear_descendants_clickable(lv_obj_t * obj)
{
    uint32_t i;
    uint32_t n = lv_obj_get_child_count(obj);
    for (i = 0; i < n; i++) {
        lv_obj_t * c = lv_obj_get_child(obj, i);
        lv_obj_remove_flag(c, LV_OBJ_FLAG_CLICKABLE);
        clear_descendants_clickable(c);
    }
}

/* LVGL sends HOVER_OVER/HOVER_LEAVE to the object under the mouse; toggle the
 * hovered state so the hover-state styles below animate in and out. */
static void hover_event_cb(lv_event_t * e)
{
    lv_obj_t * o = lv_event_get_target_obj(e);

    if (lv_event_get_code(e) == LV_EVENT_HOVER_OVER) {
        lv_obj_add_state(o, LV_STATE_HOVERED);
    }
    else {
        lv_obj_remove_state(o, LV_STATE_HOVERED);
    }
}

/* Match the HTML `.tile:hover` but make it clearly readable: brighter fill,
 * a solid green edge, a phosphor glow, and a lift. */
static void add_hover_glow(lv_obj_t * o)
{
    lv_obj_set_style_bg_color(o, lv_color_hex(G_GREEN), LV_STATE_HOVERED);
    lv_obj_set_style_bg_opa(o, 64, LV_STATE_HOVERED);
    lv_obj_set_style_border_color(o, lv_color_hex(G_GREEN), LV_STATE_HOVERED);
    lv_obj_set_style_border_opa(o, LV_OPA_COVER, LV_STATE_HOVERED);
    lv_obj_set_style_border_width(o, 3, LV_STATE_HOVERED);
    lv_obj_set_style_shadow_color(o, lv_color_hex(G_GREEN), LV_STATE_HOVERED);
    lv_obj_set_style_shadow_width(o, 26, LV_STATE_HOVERED);      /* phosphor glow */
    lv_obj_set_style_shadow_opa(o, 170, LV_STATE_HOVERED);
    lv_obj_set_style_shadow_spread(o, 1, LV_STATE_HOVERED);
    lv_obj_set_style_translate_y(o, -6, LV_STATE_HOVERED);       /* lift */
    lv_obj_set_style_transition(o, &g_hover_trans, 0);
    lv_obj_add_event_cb(o, hover_event_cb, LV_EVENT_HOVER_OVER, NULL);
    lv_obj_add_event_cb(o, hover_event_cb, LV_EVENT_HOVER_LEAVE, NULL);
}

/* Lighter hover for the chat quick-reply chips: brighter fill + edge + a small
 * glow, but no vertical lift (the chip row is tight, a lift would clip the top). */
static void add_chip_hover(lv_obj_t * o)
{
    lv_obj_set_style_bg_opa(o, 72, LV_STATE_HOVERED);
    lv_obj_set_style_border_color(o, lv_color_hex(G_GREEN), LV_STATE_HOVERED);
    lv_obj_set_style_border_opa(o, LV_OPA_COVER, LV_STATE_HOVERED);
    lv_obj_set_style_shadow_color(o, lv_color_hex(G_GREEN), LV_STATE_HOVERED);
    lv_obj_set_style_shadow_width(o, 16, LV_STATE_HOVERED);
    lv_obj_set_style_shadow_opa(o, 120, LV_STATE_HOVERED);
    lv_obj_set_style_transition(o, &g_hover_trans, 0);
    lv_obj_add_event_cb(o, hover_event_cb, LV_EVENT_HOVER_OVER, NULL);
    lv_obj_add_event_cb(o, hover_event_cb, LV_EVENT_HOVER_LEAVE, NULL);
}

/* ================= Animation callbacks ================= */
static void anim_opa_cb(void * obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)(v > 255 ? 255 : v), 0);
}

static void anim_width_pct_cb(void * obj, int32_t v)
{
    lv_obj_set_width((lv_obj_t *)obj, lv_pct(v));
}

static void anim_track_x_cb(void * obj, int32_t v)
{
    lv_obj_set_x((lv_obj_t *)obj, v);
}

/* Expand/collapse transition: the card grows from (and shrinks back into) the
 * source tile's rectangle while fading. We animate the REAL geometry (x/y/w/h)
 * instead of transform_scale - scaling a large card renders it through a square
 * layer and produces visible artifacts. This also gives the exact "zoom out of
 * the tile you tapped" feel because the start rect is that tile. */
typedef struct {
    int32_t x0, y0, w0, h0;   /* source tile rect (relative to expanded layer) */
    int32_t x1, y1, w1, h1;   /* final centered rect                            */
} exp_geo_t;

static exp_geo_t exp_geo;

static void anim_exp_cb(void * obj, int32_t v)
{
    lv_obj_t * o = (lv_obj_t *)obj;                  /* v: 0..256 progress */
    int32_t    t = v > 256 ? 256 : (v < 0 ? 0 : v);

    lv_obj_set_pos(o, exp_geo.x0 + (exp_geo.x1 - exp_geo.x0) * t / 256,
                      exp_geo.y0 + (exp_geo.y1 - exp_geo.y0) * t / 256);
    lv_obj_set_size(o, exp_geo.w0 + (exp_geo.w1 - exp_geo.w0) * t / 256,
                      exp_geo.h0 + (exp_geo.h1 - exp_geo.h0) * t / 256);
    lv_obj_set_style_opa(o, (lv_opa_t)LV_MIN(255, 90 + 200 * t / 256), 0);
}

static void anim_del_ready_cb(lv_anim_t * a)
{
    lv_obj_delete((lv_obj_t *)a->var);
}

/* Breathing "live" dot: pulse opacity 255->70 (HTML livepulse). Opacity only -
 * a transform_scale here would render the circle through a square layer and
 * flicker outside the round area, so we keep the dot's geometry fixed. */
static void anim_breath_cb(void * obj, int32_t v)
{
    lv_obj_t * d = (lv_obj_t *)obj;   /* v in [0,256]; 256 = brightest */
    lv_obj_set_style_bg_opa(d, (lv_opa_t)(70 + (255 - 70) * v / 256), 0);
}

static void start_breathing(lv_obj_t * dot)
{
    lv_anim_t a;

    lv_anim_init(&a);
    lv_anim_set_var(&a, dot);
    lv_anim_set_exec_cb(&a, anim_breath_cb);
    lv_anim_set_values(&a, 256, 0);
    lv_anim_set_duration(&a, 850);
    lv_anim_set_playback_duration(&a, 850);   /* 1.7s round-trip like the HTML */
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

/* Music equalizer bar: animate its pixel height (HTML eqbar 4px<->15px, scaled up). */
static void anim_bar_h_cb(void * obj, int32_t v)
{
    lv_obj_set_height((lv_obj_t *)obj, v);
}

/* Typing indicator dot: bob up 4px + brighten (HTML .think keyframes). */
static void anim_think_cb(void * obj, int32_t v)
{
    lv_obj_t * d = (lv_obj_t *)obj;   /* v in [0,256] */
    lv_obj_set_style_translate_y(d, -4 * v / 256, 0);
    lv_obj_set_style_bg_opa(d, (lv_opa_t)(64 + (255 - 64) * v / 256), 0);
}
/* ================= Forward declarations ================= */
static void g3_expand(g3_app_t app, lv_obj_t * src);
static void g3_collapse(void);
static void paint_live(void);

static const char * app_title(g3_app_t app)
{
    switch (app) {
        case APP_MUSIC:     return "Now Playing";
        case APP_NAVIGATE:  return "Navigate";
        case APP_AGENT:     return "AI Agent";
        case APP_ACTIVITY:  return "Activity";
        case APP_MESSAGES:  return "Alex Chen";
        case APP_TRANSLATE: return "Translate";
        case APP_CALENDAR:  return "Calendar";
        case APP_CLOCK:     return "Now";
        case APP_WEATHER:   return "Weather";
        default:            return "App";
    }
}

/* ================= Tile click -> expand ================= */
static void tile_click_cb(lv_event_t * e)
{
    lv_obj_t *  tile = lv_event_get_target_obj(e);
    g3_app_t    app  = (g3_app_t)(lv_uintptr_t)lv_obj_get_user_data(tile);

    if (g3.drag_moved || g3.expanded != NULL || g3.overlay != 0) {
        return;
    }
    g3_expand(app, tile);
}

static void tag_live_tile(lv_obj_t * tile, g3_app_t app)
{
    lv_obj_set_user_data(tile, (void *)(lv_uintptr_t)app);
    clear_descendants_clickable(tile);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tile, tile_click_cb, LV_EVENT_CLICKED, NULL);
    add_hover_glow(tile);
}

/* ================= Compact live-tile builders ================= */
static void build_music(lv_obj_t * tile)
{
    lv_obj_t * head = g3_head(tile, "NOW PLAYING", NULL, true);
    g3_eq(head);   /* dancing equalizer on the right of the head row */
    g3_grow(tile);
    g3.m_title  = g3_label(tile, "-", FONT_LG, OP_BRIGHT);
    g3.m_artist = g3_label(tile, "-", FONT_SM, OP_DIM);
    g3.m_bar    = g3_bar(tile, 0);

    lv_obj_t * row = g3_plain(tile);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    g3.m_time = g3_label(row, "0:00 / 0:00", FONT_SM, OP_DIM);
    g3_label(row, LV_SYMBOL_PLAY " playing", FONT_SM, OP_DIM);
    tag_live_tile(tile, APP_MUSIC);
}

static void build_navigate(lv_obj_t * tile)
{
    g3_head(tile, "NAVIGATE", LV_SYMBOL_GPS, true);
    g3_grow(tile);
    g3.n_eta   = g3_label(tile, "- min", FONT_XL, OP_BRIGHT);
    g3.n_dist  = g3_label(tile, "- mi - to home", FONT_SM, OP_DIM);
    g3.n_instr = g3_label(tile, "-", FONT_SM, OP_DIM);
    tag_live_tile(tile, APP_NAVIGATE);
}

static void build_agent(lv_obj_t * tile)
{
    g3_head(tile, "AI AGENT", LV_SYMBOL_EYE_OPEN, true);
    g3_grow(tile);
    g3.ag_status = g3_label(tile, "-", FONT_MD, OP_BRIGHT);
    lv_label_set_long_mode(g3.ag_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g3.ag_status, lv_pct(100));
    g3_label(tile, "tap to chat", FONT_SM, OP_DIM);
    tag_live_tile(tile, APP_AGENT);
}

static void build_activity(lv_obj_t * tile)
{
    g3_head(tile, "ACTIVITY", LV_SYMBOL_CHARGE, true);
    g3_grow(tile);
    g3.a_steps = g3_label(tile, "-", FONT_XL, OP_BRIGHT);
    g3.a_goal  = g3_label(tile, "-", FONT_SM, OP_DIM);
    g3.a_bar   = g3_bar(tile, 0);
    tag_live_tile(tile, APP_ACTIVITY);
}

static void build_messages(lv_obj_t * tile)
{
    g3_head(tile, "MESSAGES", LV_SYMBOL_ENVELOPE, true);
    g3_grow(tile);
    g3.msg_from = g3_label(tile, "-", FONT_MD, OP_BRIGHT);
    g3.msg_text = g3_label(tile, "-", FONT_SM, OP_DIM);
    lv_label_set_long_mode(g3.msg_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g3.msg_text, lv_pct(100));
    g3.msg_meta = g3_label(tile, "-", FONT_SM, OP_DIM);
    tag_live_tile(tile, APP_MESSAGES);
}

static void build_translate(lv_obj_t * tile)
{
    g3_head(tile, "TRANSLATE", LV_SYMBOL_LIST, true);
    g3_label(tile, "EN -> ZH - live captions", FONT_SM, OP_DIM);
    g3_grow(tile);
    g3.tr_src = g3_label(tile, "-", FONT_SM, OP_DIM);
    lv_label_set_long_mode(g3.tr_src, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g3.tr_src, lv_pct(100));
    g3.tr_dst = g3_label(tile, "-", g3.font_cjk != NULL ? g3.font_cjk : FONT_MD, OP_BRIGHT);
    lv_label_set_long_mode(g3.tr_dst, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g3.tr_dst, lv_pct(100));
    tag_live_tile(tile, APP_TRANSLATE);
}

static void build_calendar(lv_obj_t * tile)
{
    g3_head(tile, "CALENDAR", LV_SYMBOL_LIST, true);
    g3_grow(tile);
    g3_label(tile, "Design Review", FONT_MD, OP_BRIGHT);
    lv_obj_t * row = g3_plain(tile);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 5, 0);
    g3_label(row, "Room 4 -", FONT_SM, OP_DIM);
    g3.cal_when = g3_label(row, "-", FONT_SM, OP_DIM);
    tag_live_tile(tile, APP_CALENDAR);
}

static void build_clock(lv_obj_t * tile)
{
    g3_head(tile, "NOW", NULL, false);
    g3_grow(tile);
    g3.clk_big = g3_label(tile, "--:--", FONT_XL, OP_BRIGHT);
    g3.clk_date = g3_label(tile, "-", FONT_SM, OP_DIM);
    tag_live_tile(tile, APP_CLOCK);
}

static void build_weather(lv_obj_t * tile)
{
    g3_head(tile, "WEATHER", LV_SYMBOL_BELL, false);
    g3_grow(tile);
    g3_label(tile, "23 deg", FONT_XL, OP_BRIGHT);
    g3_label(tile, "Clear - San Francisco", FONT_SM, OP_DIM);
    g3_label(tile, "H24 L16 - feels 21", FONT_SM, OP_DIM);
    tag_live_tile(tile, APP_WEATHER);
}
/* ================= Control panel tiles ================= */
typedef struct {
    const char * ico;
    const char * label;
    bool         is_slider;
    bool         state;
    int32_t      val;
} ctrl_t;

static ctrl_t CTRLS[CTRL_COUNT] = {
    { LV_SYMBOL_UP,         "Airplane",   false, false, 0 },
    { LV_SYMBOL_BLUETOOTH,  "Bluetooth",  false, true,  0 },
    { LV_SYMBOL_MUTE,       "Do Not Dstb",false, false, 0 },
    { LV_SYMBOL_REFRESH,    "Auto-rotate",false, true,  0 },
    { LV_SYMBOL_EYE_OPEN,   "Brightness", true,  false, 70 },
    { LV_SYMBOL_VOLUME_MAX, "Volume",     true,  false, 45 },
    { LV_SYMBOL_COPY,       "Windowed",   false, true,  0 },
    { LV_SYMBOL_GPS,        "Recenter",   false, false, 0 },
};

static lv_obj_t * ctrl_tile[CTRL_COUNT];
static lv_obj_t * ctrl_val_lbl[CTRL_COUNT];
static lv_obj_t * ctrl_bar[CTRL_COUNT];

static void ctrl_apply_style(int32_t i)
{
    bool on = CTRLS[i].state;

    lv_obj_set_style_bg_opa(ctrl_tile[i], on ? 40 : 16, 0);
    lv_obj_set_style_border_opa(ctrl_tile[i], on ? OP_BRIGHT : 130, 0);
}

static void ctrl_click_cb(lv_event_t * e)
{
    int32_t i = (int32_t)(lv_uintptr_t)lv_event_get_user_data(e);

    if (g3.drag_moved) {
        return;
    }
    if (CTRLS[i].is_slider) {
        CTRLS[i].val += 20;
        if (CTRLS[i].val > 100) {
            CTRLS[i].val = 20;
        }
        lv_obj_set_width(ctrl_bar[i], lv_pct(CTRLS[i].val));
    }
    else {
        CTRLS[i].state = !CTRLS[i].state;
        lv_label_set_text(ctrl_val_lbl[i], CTRLS[i].state ? "ON" : "OFF");
        ctrl_apply_style(i);
    }
}

static void build_ctrl(lv_obj_t * parent, int32_t i)
{
    lv_obj_t * t = g3_tile(parent);

    lv_obj_set_style_pad_all(t, 12, 0);
    ctrl_tile[i] = t;
    g3_label(t, CTRLS[i].ico, FONT_MD, OP_BRIGHT);
    g3_label(t, CTRLS[i].label, FONT_SM, OP_DIM);
    g3_grow(t);
    if (CTRLS[i].is_slider) {
        ctrl_bar[i] = g3_bar(t, CTRLS[i].val);
    }
    else {
        ctrl_val_lbl[i] = g3_label(t, CTRLS[i].state ? "ON" : "OFF", FONT_SM, OP_DIM);
    }
    ctrl_apply_style(i);
    clear_descendants_clickable(t);
    lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(t, ctrl_click_cb, LV_EVENT_CLICKED, (void *)(lv_uintptr_t)i);
    add_hover_glow(t);
}

/* ================= Screen builders ================= */
static lv_obj_t * make_screen(const int32_t * cols, const int32_t * rows)
{
    lv_obj_t * s = lv_obj_create(g3.track);

    lv_obj_set_size(s, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_opa(s, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_style_radius(s, 0, 0);
    lv_obj_set_style_pad_top(s, PAD_TOP, 0);
    lv_obj_set_style_pad_bottom(s, PAD_BOTTOM, 0);
    lv_obj_set_style_pad_left(s, PAD_SIDE, 0);
    lv_obj_set_style_pad_right(s, PAD_SIDE, 0);
    lv_obj_set_style_pad_column(s, TILE_GAP, 0);
    lv_obj_set_style_pad_row(s, TILE_GAP, 0);
    lv_obj_set_style_grid_column_dsc_array(s, cols, 0);
    lv_obj_set_style_grid_row_dsc_array(s, rows, 0);
    lv_obj_set_layout(s, LV_LAYOUT_GRID);
    lv_obj_set_flex_grow(s, 0);
    lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);   /* catch drags started on empty areas */
    return s;
}

static void cell(lv_obj_t * o, int32_t c, int32_t cs, int32_t r, int32_t rs)
{
    lv_obj_set_grid_cell(o, LV_GRID_ALIGN_STRETCH, c, cs, LV_GRID_ALIGN_STRETCH, r, rs);
}

static void build_screen_control(void)
{
    static const int32_t cols[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static const int32_t rows[] = { LV_GRID_FR(2), LV_GRID_FR(3), LV_GRID_FR(3), LV_GRID_TEMPLATE_LAST };
    lv_obj_t * s = make_screen(cols, rows);
    int32_t i;

    lv_obj_t * status = g3_tile(s);
    g3_head(status, "SYSTEM", LV_SYMBOL_BARS, false);
    g3_label(status, "All systems nominal", FONT_MD, OP_BRIGHT);
    g3_label(status, "Battery 82% - 5h 20m - G3 firmware 1.3", FONT_SM, OP_DIM);
    cell(status, 0, 4, 0, 1);

    for (i = 0; i < CTRL_COUNT; i++) {
        build_ctrl(s, i);
        cell(ctrl_tile[i], i % 4, 1, 1 + (i / 4), 1);
    }
}

static void build_screen_dashboard(void)
{
    static const int32_t cols[] = { LV_GRID_FR(13), LV_GRID_FR(10), LV_GRID_TEMPLATE_LAST };
    static const int32_t rows[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    lv_obj_t * s = make_screen(cols, rows);
    lv_obj_t * t;

    t = g3_tile(s); build_music(t);    cell(t, 0, 1, 0, 1);
    t = g3_tile(s); build_navigate(t); cell(t, 1, 1, 0, 1);
    t = g3_tile(s); build_agent(t);    cell(t, 0, 1, 1, 1);
    t = g3_tile(s); build_activity(t); cell(t, 1, 1, 1, 1);
}

static void build_screen_desk1(void)
{
    static const int32_t cols[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static const int32_t rows[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    lv_obj_t * s = make_screen(cols, rows);
    lv_obj_t * t;

    t = g3_tile(s); build_messages(t);  cell(t, 0, 1, 0, 2);
    t = g3_tile(s); build_translate(t); cell(t, 1, 1, 0, 1);
    t = g3_tile(s); build_calendar(t);  cell(t, 1, 1, 1, 1);
}

static void build_screen_desk2(void)
{
    static const int32_t cols[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static const int32_t rows[] = { LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    lv_obj_t * s = make_screen(cols, rows);
    lv_obj_t * t;

    t = g3_tile(s); build_clock(t);   cell(t, 0, 1, 0, 1);
    t = g3_tile(s); build_weather(t); cell(t, 1, 1, 0, 1);
}
/* ================= Navigation ================= */
static const char * const SCREEN_NAMES[SCREEN_COUNT] = {
    "Control Panel", "Dashboard", "Home Desk 1", "Home Desk 2",
};

static void update_chrome(void)
{
    int32_t i;
    const char * name;

    if (g3.overlay == 1) {
        name = "Notifications";
    }
    else if (g3.overlay == 2) {
        name = "Apps";
    }
    else {
        name = SCREEN_NAMES[g3.index];
    }
    lv_label_set_text_fmt(g3.sb_left, "G3 - %s", name);

    for (i = 0; i < SCREEN_COUNT; i++) {
        bool on = (i == g3.index);
        lv_obj_set_style_bg_opa(g3.pager_dot[i], on ? OP_BRIGHT : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_opa(g3.pager_dot[i], on ? OP_BRIGHT : OP_DIM, 0);
    }
}

static void anim_track_to(int32_t x)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, g3.track);
    lv_anim_set_exec_cb(&a, anim_track_x_cb);
    lv_anim_set_values(&a, lv_obj_get_x(g3.track), x);
    lv_anim_set_duration(&a, 500);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void go_to(int32_t i)
{
    if (g3.overlay != 0 || g3.expanded != NULL) {
        return;
    }
    if (i < 0) {
        i = 0;
    }
    if (i > SCREEN_COUNT - 1) {
        i = SCREEN_COUNT - 1;
    }
    g3.index = i;
    anim_track_to(-i * SCREEN_W);
    update_chrome();
}

/* ================= Overlays ================= */
static void anim_overlay_to(lv_obj_t * ov, int32_t y)
{
    lv_anim_t a;

    lv_anim_init(&a);
    lv_anim_set_var(&a, ov);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, lv_obj_get_y(ov), y);
    lv_anim_set_duration(&a, 450);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void open_overlay(int32_t kind)
{
    if (g3.overlay != 0 || g3.expanded != NULL) {
        return;
    }
    g3.overlay = kind;
    anim_overlay_to(kind == 1 ? g3.ov_notify : g3.ov_apps, 0);
    lv_obj_set_style_opa(g3.carousel, 60, 0);
    update_chrome();
}

static void close_overlay(void)
{
    if (g3.overlay == 0) {
        return;
    }
    anim_overlay_to(g3.ov_notify, -SCREEN_H);
    anim_overlay_to(g3.ov_apps, SCREEN_H);
    lv_obj_set_style_opa(g3.carousel, OP_BRIGHT, 0);
    g3.overlay = 0;
    update_chrome();
}

static lv_obj_t * make_overlay(int32_t off_y, const char * title)
{
    lv_obj_t * ov = lv_obj_create(lv_screen_active());

    lv_obj_set_size(ov, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(ov, 0, off_y);
    lv_obj_set_style_bg_color(ov, lv_color_hex(0x020604), 0);
    lv_obj_set_style_bg_opa(ov, 235, 0);
    lv_obj_set_style_border_width(ov, 0, 0);
    lv_obj_set_style_radius(ov, 0, 0);
    lv_obj_set_style_pad_top(ov, 50, 0);
    lv_obj_set_style_pad_bottom(ov, 34, 0);
    lv_obj_set_style_pad_left(ov, 36, 0);
    lv_obj_set_style_pad_right(ov, 36, 0);
    lv_obj_set_style_pad_row(ov, 12, 0);
    lv_obj_set_flex_flow(ov, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ov, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t * t = g3_label(ov, title, FONT_SM, OP_DIM);
    (void)t;
    return ov;
}

static void build_overlay_notify(void)
{
    int32_t i;

    g3.ov_notify = make_overlay(-SCREEN_H, "NOTIFICATIONS");

    for (i = 0; i < NOTIF_COUNT; i++) {
        const notif_t * n = &NOTIFS[i];
        lv_obj_t * row = g3_plain(g3.ov_notify);

        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(row, 12, 0);
        if (n->summary) {
            lv_obj_set_style_bg_color(row, lv_color_hex(G_GREEN), 0);
            lv_obj_set_style_bg_opa(row, 16, 0);
            lv_obj_set_style_border_color(row, lv_color_hex(G_GREEN), 0);
            lv_obj_set_style_border_opa(row, 130, 0);
            lv_obj_set_style_border_width(row, 1, 0);
            lv_obj_set_style_radius(row, 12, 0);
            lv_obj_set_style_pad_all(row, 14, 0);
        }

        lv_obj_t * ico = g3_label(row, n->ico, FONT_MD, OP_BRIGHT);
        (void)ico;

        lv_obj_t * body = g3_plain(row);
        lv_obj_set_flex_grow(body, 1);
        lv_obj_set_height(body, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(body, 2, 0);

        lv_obj_t * head = g3_plain(body);
        lv_obj_set_width(head, lv_pct(100));
        lv_obj_set_height(head, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        g3_label(head, n->from, FONT_SM, OP_DIM);
        g3_label(head, n->time, FONT_SM, OP_FAINT);

        lv_obj_t * txt = g3_label(body, n->text, FONT_SM, OP_BRIGHT);
        lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(txt, lv_pct(100));
    }
}

static void build_overlay_apps(void)
{
    static const int32_t cols[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static const int32_t rows[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    int32_t i;

    g3.ov_apps = make_overlay(SCREEN_H, "APPS");

    lv_obj_t * grid = g3_plain(g3.ov_apps);
    lv_obj_set_width(grid, lv_pct(100));
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_style_pad_column(grid, 16, 0);
    lv_obj_set_style_pad_row(grid, 16, 0);
    lv_obj_set_style_grid_column_dsc_array(grid, cols, 0);
    lv_obj_set_style_grid_row_dsc_array(grid, rows, 0);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);

    for (i = 0; i < APP_COUNT; i++) {
        lv_obj_t * app = g3_tile(grid);
        lv_obj_set_flex_align(app, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(app, 10, 0);
        g3_label(app, APPS[i].ico, FONT_LG, OP_BRIGHT);
        g3_label(app, APPS[i].name, FONT_SM, OP_DIM);
        cell(app, i % 4, 1, i / 4, 1);
    }
}
/* ================= Expand / collapse ================= */
static const char * lbl_text(lv_obj_t * l)
{
    return (l != NULL) ? lv_label_get_text(l) : "-";
}

static void exp_line(lv_obj_t * p, const char * txt, const lv_font_t * font, lv_opa_t opa)
{
    lv_obj_t * l = g3_label(p, txt, font, opa);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, lv_pct(100));
}

static void exp_click_cb(lv_event_t * e)
{
    (void)e;
    g3_collapse();
}

static void exp_layer_click_cb(lv_event_t * e)
{
    if (lv_event_get_target_obj(e) == g3.expanded_layer) {
        g3_collapse();
    }
}

/* ================= Simulated chat (AI Agent + Messages) ================= */
static bool        agent_thinking;
static char        agent_pending[CHAT_TEXT_MAX];

static chat_msg_t * chat_arr(g3_app_t app, int32_t ** n_out)
{
    if (app == APP_AGENT) {
        *n_out = &agent_n;
        return agent_hist;
    }
    *n_out = &msgs_n;
    return msgs_hist;
}

static void chat_push(g3_app_t app, bool me, const char * text)
{
    int32_t *    n;
    chat_msg_t * arr = chat_arr(app, &n);

    if (*n >= CHAT_MAX) {                        /* ring: drop the oldest line */
        memmove(arr, arr + 1, (CHAT_MAX - 1) * sizeof(chat_msg_t));
        (*n)--;
    }
    arr[*n].me = me;
    snprintf(arr[*n].text, CHAT_TEXT_MAX, "%s", text);
    (*n)++;
}

/* One chat bubble inside a full-width alignment row (them = left, me = right). */
static void make_bubble(lv_obj_t * log, bool me, const char * text)
{
    lv_obj_t * row = g3_plain(log);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, me ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * b = lv_obj_create(row);
    lv_obj_set_width(b, LV_SIZE_CONTENT);
    lv_obj_set_height(b, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(b, 700, 0);
    lv_obj_set_style_radius(b, 13, 0);
    lv_obj_set_style_pad_hor(b, 15, 0);
    lv_obj_set_style_pad_ver(b, 10, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(G_GREEN), 0);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    if (me) {
        lv_obj_set_style_bg_opa(b, 60, 0);
        lv_obj_set_style_border_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_color(b, lv_color_hex(G_GREEN), 0);
        lv_obj_set_style_shadow_width(b, 12, 0);
        lv_obj_set_style_shadow_opa(b, 80, 0);
    }
    else {
        lv_obj_set_style_bg_opa(b, 20, 0);
        lv_obj_set_style_border_opa(b, 100, 0);
    }

    lv_obj_t * l = g3_label(b, text, FONT_SM, OP_BRIGHT);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(l, 668, 0);
}

/* The animated "typing..." bubble (three bobbing think dots). */
static void add_typing(lv_obj_t * log)
{
    int32_t i;
    lv_obj_t * row = g3_plain(log);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t * b = lv_obj_create(row);
    lv_obj_set_size(b, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(b, 13, 0);
    lv_obj_set_style_pad_hor(b, 15, 0);
    lv_obj_set_style_pad_ver(b, 13, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_border_opa(b, 100, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_bg_opa(b, 20, 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(b, 6, 0);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);

    for (i = 0; i < 3; i++) {
        lv_anim_t  a;
        lv_obj_t * dot = lv_obj_create(b);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(G_GREEN), 0);
        lv_obj_set_style_bg_opa(dot, 64, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

        lv_anim_init(&a);
        lv_anim_set_var(&a, dot);
        lv_anim_set_exec_cb(&a, anim_think_cb);
        lv_anim_set_values(&a, 0, 256);
        lv_anim_set_duration(&a, 650);
        lv_anim_set_playback_duration(&a, 650);
        lv_anim_set_delay(&a, i * 200);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_start(&a);
    }
}

static void chat_render(void)
{
    int32_t      i;
    int32_t *    n;
    chat_msg_t * arr;
    uint32_t     cc;

    if (chat_log == NULL) {
        return;
    }
    lv_obj_clean(chat_log);

    arr = chat_arr(chat_open_app, &n);
    for (i = 0; i < *n; i++) {
        make_bubble(chat_log, arr[i].me, arr[i].text);
    }
    if (chat_open_app == APP_AGENT && agent_thinking) {
        add_typing(chat_log);
    }

    lv_obj_update_layout(chat_log);
    cc = lv_obj_get_child_count(chat_log);
    if (cc > 0) {
        lv_obj_scroll_to_view(lv_obj_get_child(chat_log, cc - 1), LV_ANIM_OFF);
    }
}

/* Port of the HTML agentReply(): keyword -> canned assistant answer. */
static const char * agent_reply(const char * t)
{
    char   low[CHAT_TEXT_MAX];
    size_t i;
    size_t len = 0;

    for (i = 0; t[i] != '\0' && i < CHAT_TEXT_MAX - 1; i++) {
        low[i] = (char)tolower((unsigned char)t[i]);
    }
    low[i] = '\0';
    len = i;

    if (strstr(low, "weather") || strstr(low, "rain") || strstr(low, "temp") || strstr(low, "forecast"))
        return "It's 23 and clear in San Francisco - high of 24, no rain expected.";
    if (strstr(low, "draft") || strstr(low, "reply") || strstr(low, "respond"))
        return "Drafted: \"Thanks Alex - merging now, I'll keep an eye on the deploy.\"  Want me to send it?";
    if (strstr(low, "calendar") || strstr(low, "meeting") || strstr(low, "schedule") || strstr(low, "next") || strstr(low, "11"))
        return "Your next event is Design Review at 11:00 in Room 4 - about 20 minutes away.";
    if (strstr(low, "summar") || strstr(low, "inbox") || strstr(low, "morning") || strstr(low, "today"))
        return "Today: 2 messages to reply to, a meeting at 11, and 3 low-priority updates. Nothing urgent.";
    if (strstr(low, "nav") || strstr(low, "home") || strstr(low, "route") || strstr(low, "traffic") || strstr(low, "drive"))
        return "Traffic's light - roughly 14 minutes home. Want me to start navigation?";
    if (strstr(low, "music") || strstr(low, "play") || strstr(low, "song") || strstr(low, "pause") || strstr(low, "track"))
        return "Now playing \"Nightcall\" by Kavinsky. I can skip or pause if you'd like.";
    if (strstr(low, "thank") || strstr(low, "great") || strstr(low, "nice") || strstr(low, "cool") || strstr(low, "awesome") || strstr(low, "perfect"))
        return "Anytime - I'll keep working in the background.";
    if (len > 0 && low[len - 1] == '?')
        return "Good question - from what I can see, everything's on track for this morning.";
    return "On it - I'll take care of that and keep you posted. Anything else?";
}

static void chat_reply_cb(lv_timer_t * t)
{
    g3_app_t app = (g3_app_t)(lv_uintptr_t)lv_timer_get_user_data(t);

    lv_timer_delete(t);
    chat_reply_timer = NULL;

    if (app == APP_AGENT) {
        agent_thinking = false;
        chat_push(APP_AGENT, false, agent_pending);
    }
    else {
        chat_push(APP_MESSAGES, false, ALEX_REPLIES[alex_idx]);
        alex_idx = (alex_idx + 1) % ALEX_COUNT;
    }
    if (chat_open_app == app) {
        chat_render();
    }
}

static void chat_send(g3_app_t app, const char * text)
{
    chat_push(app, true, text);

    if (app == APP_AGENT) {
        snprintf(agent_pending, CHAT_TEXT_MAX, "%s", agent_reply(text));
        agent_thinking = true;
    }
    if (chat_open_app == app) {
        chat_render();
    }
    if (chat_reply_timer != NULL) {
        lv_timer_delete(chat_reply_timer);
    }
    chat_reply_timer = lv_timer_create(chat_reply_cb, app == APP_AGENT ? 1100 : 900,
                                       (void *)(lv_uintptr_t)app);
}

static void chip_click_cb(lv_event_t * e)
{
    const char * text = (const char *)lv_event_get_user_data(e);
    chat_send(chat_open_app, text);
}

static void chat_close_cb(lv_event_t * e)
{
    (void)e;
    g3_collapse();
}

/* Seed both chat histories with the opening conversation (from the HTML). */
static void seed_chats(void)
{
    agent_n = 0;
    chat_push(APP_AGENT, false,
              "Good morning. I went through your inbox - 2 messages need a reply and your first meeting is at 11.");
    chat_push(APP_AGENT, true, "anything from Alex?");
    chat_push(APP_AGENT, false,
              "Yes - Alex approved the build: \"ship it - build looks great.\"  Want me to draft a reply?");

    msgs_n = 0;
    chat_push(APP_MESSAGES, false, "morning! did the build pass?");
    chat_push(APP_MESSAGES, true,  "yep, all green");
    chat_push(APP_MESSAGES, false, "nice, perf numbers?");
    chat_push(APP_MESSAGES, true,  "p95 down 12%");
    chat_push(APP_MESSAGES, false, "ship it - build looks great");
}

/* Builds the full chat window (header + scrolling bubble log + quick-reply chips)
 * into the expanded tile for the Agent / Messages apps. */
static void build_chat_view(lv_obj_t * exp, g3_app_t app)
{
    const char * const * chips = (app == APP_AGENT) ? AGENT_CHIPS : MSGS_CHIPS;
    int32_t i;

    chat_open_app = app;

    /* header: breathing dot + title on the left, a close affordance on the right */
    lv_obj_t * head = g3_plain(exp);
    lv_obj_set_width(head, lv_pct(100));
    lv_obj_set_height(head, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t * hleft = g3_plain(head);
    lv_obj_set_size(hleft, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(hleft, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hleft, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hleft, 9, 0);

    lv_obj_t * dot = lv_obj_create(hleft);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(G_GREEN), 0);
    lv_obj_set_style_bg_opa(dot, OP_BRIGHT, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    start_breathing(dot);
    g3_label(hleft, app_title(app), FONT_MD, OP_BRIGHT);

    lv_obj_t * close = g3_label(head, LV_SYMBOL_CLOSE "  close", FONT_SM, OP_DIM);
    lv_obj_add_flag(close, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(close, chat_close_cb, LV_EVENT_CLICKED, NULL);

    /* scrolling bubble log */
    chat_log = lv_obj_create(exp);
    lv_obj_set_width(chat_log, lv_pct(100));
    lv_obj_set_flex_grow(chat_log, 1);
    lv_obj_set_style_bg_opa(chat_log, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chat_log, 0, 0);
    lv_obj_set_style_radius(chat_log, 0, 0);
    lv_obj_set_style_pad_all(chat_log, 0, 0);
    lv_obj_set_style_pad_right(chat_log, 8, 0);
    lv_obj_set_style_pad_row(chat_log, 8, 0);
    lv_obj_set_flex_flow(chat_log, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(chat_log, LV_DIR_VER);

    /* quick-reply chips (tap to "type" a message in the simulator) */
    lv_obj_t * chip_row = g3_plain(exp);
    lv_obj_set_width(chip_row, lv_pct(100));
    lv_obj_set_height(chip_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(chip_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(chip_row, 10, 0);
    /* let the hover lift + glow spill past the row instead of being clipped */
    lv_obj_add_flag(chip_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    for (i = 0; i < CHIP_COUNT; i++) {
        lv_obj_t * chip = lv_obj_create(chip_row);
        lv_obj_set_size(chip, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_radius(chip, 10, 0);
        lv_obj_set_style_pad_hor(chip, 16, 0);
        lv_obj_set_style_pad_ver(chip, 10, 0);
        lv_obj_set_style_bg_color(chip, lv_color_hex(G_GREEN), 0);
        lv_obj_set_style_bg_opa(chip, 24, 0);
        lv_obj_set_style_border_color(chip, lv_color_hex(G_GREEN), 0);
        lv_obj_set_style_border_opa(chip, 130, 0);
        lv_obj_set_style_border_width(chip, 1, 0);
        lv_obj_remove_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(chip, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(chip, chip_click_cb, LV_EVENT_CLICKED, (void *)chips[i]);
        add_chip_hover(chip);
        g3_label(chip, chips[i], FONT_SM, OP_BRIGHT);
    }

    chat_render();
}

static void g3_expand(g3_app_t app, lv_obj_t * src)
{
    bool       is_chat = (app == APP_AGENT || app == APP_MESSAGES);
    lv_obj_t * exp = g3_tile(g3.expanded_layer);
    int32_t    w   = (SCREEN_W * 86) / 100;
    int32_t    h   = (SCREEN_H * 80) / 100;

    /* Build/measure the content at the FINAL size; the open animation then
     * drives the real geometry from the tapped tile's rect up to this size. */
    lv_obj_set_align(exp, LV_ALIGN_TOP_LEFT);
    lv_obj_set_size(exp, w, h);
    lv_obj_set_style_pad_all(exp, 28, 0);
    lv_obj_set_style_pad_row(exp, 12, 0);

    if (is_chat) {
        build_chat_view(exp, app);
        goto finalize;
    }

    exp_line(exp, app_title(app), FONT_MD, OP_DIM);
    g3_grow(exp);

    switch (app) {
        case APP_MUSIC:
            exp_line(exp, lbl_text(g3.m_title), FONT_XL, OP_BRIGHT);
            exp_line(exp, lbl_text(g3.m_artist), FONT_MD, OP_DIM);
            exp_line(exp, lbl_text(g3.m_time), FONT_SM, OP_DIM);
            break;
        case APP_NAVIGATE:
            exp_line(exp, lbl_text(g3.n_eta), FONT_XL, OP_BRIGHT);
            exp_line(exp, lbl_text(g3.n_dist), FONT_MD, OP_DIM);
            exp_line(exp, lbl_text(g3.n_instr), FONT_MD, OP_DIM);
            break;
        case APP_AGENT:
            exp_line(exp, lbl_text(g3.ag_status), FONT_MD, OP_BRIGHT);
            exp_line(exp, "Your always-on assistant keeps working in the background.",
                     FONT_SM, OP_DIM);
            break;
        case APP_ACTIVITY:
            exp_line(exp, lbl_text(g3.a_steps), FONT_XL, OP_BRIGHT);
            exp_line(exp, lbl_text(g3.a_goal), FONT_MD, OP_DIM);
            break;
        case APP_MESSAGES:
            exp_line(exp, lbl_text(g3.msg_from), FONT_LG, OP_BRIGHT);
            exp_line(exp, lbl_text(g3.msg_text), FONT_MD, OP_DIM);
            exp_line(exp, lbl_text(g3.msg_meta), FONT_SM, OP_FAINT);
            break;
        case APP_TRANSLATE: {
            lv_obj_t * dl;
            exp_line(exp, "EN -> ZH - live captions", FONT_SM, OP_DIM);
            exp_line(exp, lbl_text(g3.tr_src), FONT_MD, OP_DIM);
            dl = g3_label(exp, lbl_text(g3.tr_dst), g3.font_cjk != NULL ? g3.font_cjk : FONT_LG,
                          OP_BRIGHT);
            lv_label_set_long_mode(dl, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(dl, lv_pct(100));
            break;
        }
        case APP_CALENDAR:
            exp_line(exp, "Design Review", FONT_LG, OP_BRIGHT);
            exp_line(exp, "Room 4", FONT_MD, OP_DIM);
            exp_line(exp, lbl_text(g3.cal_when), FONT_MD, OP_DIM);
            break;
        case APP_CLOCK:
            exp_line(exp, lbl_text(g3.clk_big), FONT_XL, OP_BRIGHT);
            exp_line(exp, lbl_text(g3.clk_date), FONT_MD, OP_DIM);
            break;
        case APP_WEATHER:
            exp_line(exp, "23 deg", FONT_XL, OP_BRIGHT);
            exp_line(exp, "Clear - San Francisco - feels 21", FONT_MD, OP_DIM);
            break;
        default:
            break;
    }

    g3_grow(exp);
    exp_line(exp, "tap to close", FONT_SM, OP_FAINT);

finalize:
    lv_obj_update_layout(exp);

    /* Chat tiles are interactive (chips, scrolling, close), so tapping the tile
     * body must NOT collapse it - only the close button or the backdrop do. */
    if (!is_chat) {
        lv_obj_add_flag(exp, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(exp, exp_click_cb, LV_EVENT_CLICKED, NULL);
    }
    lv_obj_add_flag(g3.expanded_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(g3.carousel, 60, 0);
    g3.expanded = exp;

    /* Compute the start rect (tapped tile) and final rect (centered), both
     * relative to the expanded layer, then animate the real geometry. */
    {
        lv_area_t src_a, lay_a;
        lv_obj_get_coords(src, &src_a);
        lv_obj_get_coords(g3.expanded_layer, &lay_a);

        exp_geo.x0 = src_a.x1 - lay_a.x1;
        exp_geo.y0 = src_a.y1 - lay_a.y1;
        exp_geo.w0 = lv_area_get_width(&src_a);
        exp_geo.h0 = lv_area_get_height(&src_a);
        exp_geo.x1 = (SCREEN_W - w) / 2;
        exp_geo.y1 = (SCREEN_H - h) / 2;
        exp_geo.w1 = w;
        exp_geo.h1 = h;
    }
    anim_exp_cb(exp, 0);   /* start at the tile rect, no flash */

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, exp);
    lv_anim_set_exec_cb(&a, anim_exp_cb);
    lv_anim_set_values(&a, 0, 256);
    lv_anim_set_duration(&a, 300);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void g3_collapse(void)
{
    lv_obj_t * exp = g3.expanded;

    if (exp == NULL) {
        return;
    }
    g3.expanded = NULL;
    /* the chat widgets live inside the tile we're about to delete */
    chat_log      = NULL;
    chat_open_app = APP_NONE;
    lv_obj_remove_flag(g3.expanded_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(g3.carousel, OP_BRIGHT, 0);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, exp);
    lv_anim_set_exec_cb(&a, anim_exp_cb);
    lv_anim_set_values(&a, 256, 0);   /* shrink back into the source tile rect */
    lv_anim_set_duration(&a, 220);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_completed_cb(&a, anim_del_ready_cb);
    lv_anim_start(&a);
}

/* ================= Pointer input (mouse click + drag) ================= */
#define DRAG_MOVE_THRESH  6
#define DRAG_SNAP_THRESH  (SCREEN_W / 5)
#define DRAG_VERT_THRESH  (SCREEN_H / 6)

static int32_t drag_clamp_x(int32_t x)
{
    int32_t min_x = -(SCREEN_COUNT - 1) * SCREEN_W - 70;   /* allow a little overscroll */
    int32_t max_x = 70;

    if (x < min_x) {
        x = min_x;
    }
    if (x > max_x) {
        x = max_x;
    }
    return x;
}

/* Handles press / drag-follow / release-snap for the whole HUD via event bubbling. */
static void root_input_cb(lv_event_t * e)
{
    lv_event_code_t code  = lv_event_get_code(e);
    lv_indev_t *    indev = lv_indev_active();
    lv_point_t      p;
    int32_t         dx, dy;

    if (indev == NULL) {
        return;
    }
    lv_indev_get_point(indev, &p);

    switch (code) {
        case LV_EVENT_PRESSED:
            lv_anim_delete(g3.track, anim_track_x_cb);   /* take over any running snap */
            g3.drag_start_x  = p.x;
            g3.drag_start_y  = p.y;
            g3.track_start_x = lv_obj_get_x(g3.track);
            g3.drag_moved    = false;
            g3.dragging_h    = false;
            break;

        case LV_EVENT_PRESSING:
            if (g3.expanded != NULL) {
                break;
            }
            dx = p.x - g3.drag_start_x;
            dy = p.y - g3.drag_start_y;
            if (!g3.drag_moved &&
                (LV_ABS(dx) > DRAG_MOVE_THRESH || LV_ABS(dy) > DRAG_MOVE_THRESH)) {
                g3.drag_moved = true;
                g3.dragging_h = (LV_ABS(dx) >= LV_ABS(dy));
            }
            if (g3.drag_moved && g3.dragging_h && g3.overlay == 0) {
                lv_obj_set_x(g3.track, drag_clamp_x(g3.track_start_x + dx));
            }
            break;

        case LV_EVENT_RELEASED:
        case LV_EVENT_PRESS_LOST:
            if (g3.expanded != NULL || !g3.drag_moved) {
                break;   /* a tap: handled by the click callbacks */
            }
            dx = p.x - g3.drag_start_x;
            dy = p.y - g3.drag_start_y;
            if (g3.dragging_h) {
                if (g3.overlay == 0) {
                    int32_t target = g3.index;
                    if (dx <= -DRAG_SNAP_THRESH) {
                        target = g3.index + 1;
                    }
                    else if (dx >= DRAG_SNAP_THRESH) {
                        target = g3.index - 1;
                    }
                    go_to(target);   /* clamps + animates from the dragged position */
                }
            }
            else {
                if (g3.overlay != 0) {
                    close_overlay();
                }
                else if (dy >= DRAG_VERT_THRESH) {
                    open_overlay(1);   /* drag down -> notifications shade (from top) */
                }
                else if (dy <= -DRAG_VERT_THRESH) {
                    open_overlay(2);   /* drag up -> apps drawer (from bottom) */
                }
            }
            break;

        default:
            break;
    }
}

/* Mouse-wheel / trackpad scroll. A plain PC mouse only has a vertical wheel, so
 * vertical scrolling drives the primary navigation (screen switching); an open
 * overlay closes on any scroll. Debounced so one notch = one step. Overlays are
 * still reachable by dragging. Wheel is ignored while a tile is expanded so the
 * chat log can be scrolled by dragging inside it. */
void demo_g3_os_wheel(int32_t dx, int32_t dy)
{
    static uint32_t last_tick = 0;

    if (g3.boot != NULL) {
        return;                         /* ignore during boot */
    }
    if (lv_tick_elaps(last_tick) < 260) {
        return;                         /* debounce rapid wheel events */
    }
    if (g3.expanded != NULL) {
        return;                         /* don't hijack the expanded view */
    }

    last_tick = lv_tick_get();

    if (g3.overlay != 0) {
        close_overlay();
        return;
    }
    if (dy != 0) {
        go_to(g3.index + (dy < 0 ? 1 : -1));   /* wheel down -> next screen */
    }
    else if (dx != 0) {
        go_to(g3.index + (dx > 0 ? 1 : -1));   /* tilt/trackpad horizontal */
    }
}

/* ================= Chrome (status bar, pager) ================= */
static void build_chrome(void)
{
    lv_obj_t * top = lv_layer_top();
    int32_t    i;

    lv_obj_remove_flag(top, LV_OBJ_FLAG_SCROLLABLE);

    g3.sb_left = g3_label(top, "G3 - Dashboard", FONT_SM, OP_DIM);
    lv_obj_align(g3.sb_left, LV_ALIGN_TOP_LEFT, PAD_SIDE, 44);

    lv_obj_t * right = g3_plain(top);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(right, LV_ALIGN_TOP_RIGHT, -PAD_SIDE, 42);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(right, 14, 0);
    g3.clock_lbl = g3_label(right, "9:53", FONT_SM, OP_DIM);
    g3.batt_lbl  = g3_label(right, LV_SYMBOL_BATTERY_3 " 82%", FONT_SM, OP_DIM);

    /* Navigation is fully drag-driven (no on-screen direction buttons). */
    lv_obj_t * pager = g3_plain(top);
    lv_obj_set_size(pager, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(pager, LV_ALIGN_BOTTOM_MID, 0, -48);
    lv_obj_set_flex_flow(pager, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(pager, 9, 0);
    for (i = 0; i < SCREEN_COUNT; i++) {
        lv_obj_t * dot = lv_obj_create(pager);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(G_GREEN), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(dot, lv_color_hex(G_GREEN), 0);
        lv_obj_set_style_border_opa(dot, OP_DIM, 0);
        lv_obj_set_style_border_width(dot, 1, 0);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        g3.pager_dot[i] = dot;
    }
}

/* ================= Live updates ================= */
static const char * const DOW = "SunMonTueWedThuFriSat";
static const char * const MON = "JanFebMarAprMayJunJulAugSepOctNovDec";

static void fmt_mmss(char * buf, size_t n, int32_t s)
{
    if (s < 0) {
        s = 0;
    }
    snprintf(buf, n, "%d:%02d", s / 60, s % 60);
}

static void paint_live(void)
{
    char buf[96];
    char b2[32];

    /* Clock / date follow the real system time. */
    {
        time_t      now = time(NULL);
        struct tm * lt  = localtime(&now);
        int32_t     hh = lt ? lt->tm_hour : 0;
        int32_t     mm = lt ? lt->tm_min  : 0;
        int32_t     ss = lt ? lt->tm_sec  : 0;

        if (g3.clock_lbl != NULL) {
            lv_label_set_text_fmt(g3.clock_lbl, "%d:%02d", hh, mm);
        }
        if (g3.clk_big != NULL) {
            lv_label_set_text_fmt(g3.clk_big, "%d:%02d:%02d", hh, mm, ss);
        }
        if (g3.clk_date != NULL && lt != NULL) {
            lv_label_set_text_fmt(g3.clk_date, "%.3s %.3s %d",
                                  DOW + lt->tm_wday * 3, MON + lt->tm_mon * 3, lt->tm_mday);
        }
    }

    /* music */
    {
        const track_t * tr = &TRACKS[g3.music_idx];
        char cur[16], dur[16];
        if (g3.m_title != NULL) lv_label_set_text(g3.m_title, tr->title);
        if (g3.m_artist != NULL) lv_label_set_text(g3.m_artist, tr->artist);
        fmt_mmss(cur, sizeof(cur), g3.music_t);
        fmt_mmss(dur, sizeof(dur), tr->dur);
        if (g3.m_time != NULL) {
            snprintf(buf, sizeof(buf), "%s / %s", cur, dur);
            lv_label_set_text(g3.m_time, buf);
        }
        if (g3.m_bar != NULL) {
            lv_obj_set_width(g3.m_bar, lv_pct(g3.music_t * 100 / tr->dur));
        }
    }

    /* activity */
    if (g3.a_steps != NULL) {
        if (g3.steps >= 1000) {
            lv_label_set_text_fmt(g3.a_steps, "%d,%03d", g3.steps / 1000, g3.steps % 1000);
        }
        else {
            lv_label_set_text_fmt(g3.a_steps, "%d", g3.steps);
        }
    }
    if (g3.a_goal != NULL) {
        lv_label_set_text_fmt(g3.a_goal, "%d%% of daily goal", g3.steps * 100 / g3.goal);
    }
    if (g3.a_bar != NULL) {
        lv_obj_set_width(g3.a_bar, lv_pct(g3.steps * 100 / g3.goal));
    }

    /* navigate */
    if (g3.n_eta != NULL) {
        lv_label_set_text_fmt(g3.n_eta, "%d min", (g3.nav_eta + 59) / 60);
    }
    if (g3.n_dist != NULL) {
        lv_label_set_text_fmt(g3.n_dist, "%d.%d mi - to home",
                              g3.nav_dist_x10 / 10, g3.nav_dist_x10 % 10);
    }
    if (g3.n_instr != NULL) {
        lv_label_set_text(g3.n_instr, NAV_INSTRS[g3.nav_instr]);
    }

    /* agent (static status) */
    if (g3.ag_status != NULL) {
        lv_label_set_text(g3.ag_status,
                          "Alex approved the build. Want me to draft a reply?");
    }

    /* messages */
    if (g3.msg_from != NULL) lv_label_set_text(g3.msg_from, "Alex Chen");
    if (g3.msg_text != NULL) lv_label_set_text(g3.msg_text, "\"ship it - build looks great\"");
    if (g3.msg_meta != NULL) {
        int32_t a = g3.msg_age;
        if (a < 60) snprintf(b2, sizeof(b2), "%ds", a);
        else if (a < 3600) snprintf(b2, sizeof(b2), "%dm", a / 60);
        else snprintf(b2, sizeof(b2), "%dh", a / 3600);
        lv_label_set_text_fmt(g3.msg_meta, "%d unread - %s", g3.msg_unread, b2);
    }

    /* translate */
    if (g3.tr_src != NULL) lv_label_set_text(g3.tr_src, TRANSLATE_SRC[g3.trans_i]);
    if (g3.tr_dst != NULL) {
        lv_label_set_text(g3.tr_dst, g3.font_cjk != NULL ? TRANSLATE_DST_CJK[g3.trans_i]
                                                         : TRANSLATE_DST_PY[g3.trans_i]);
    }

    /* calendar */
    if (g3.cal_when != NULL) {
        if (g3.cal_mins > 0) {
            lv_label_set_text_fmt(g3.cal_when, "in %d min", g3.cal_mins);
        }
        else {
            lv_label_set_text(g3.cal_when, "starting now");
        }
    }
    (void)DOW; (void)MON;
}

static void live_tick(lv_timer_t * t)
{
    (void)t;
    g3.sec++;
    g3.music_t++;
    if (g3.music_t >= TRACKS[g3.music_idx].dur) {
        g3.music_idx = (g3.music_idx + 1) % TRACK_COUNT;
        g3.music_t = 0;
    }
    g3.steps += 3;
    if (g3.steps > g3.goal) {
        g3.steps = g3.goal;
    }
    if (g3.nav_eta > 0) {
        g3.nav_eta--;
    }
    g3.nav_instr = g3.nav_eta > 620 ? 0 : (g3.nav_eta > 430 ? 1 : (g3.nav_eta > 120 ? 2 : 3));
    if (g3.sec % 12U == 0U) {
        g3.trans_i = (g3.trans_i + 1) % TRANSLATE_COUNT;
    }
    if (g3.sec % 60U == 0U && g3.cal_mins > 0) {
        g3.cal_mins--;
    }
    g3.msg_age++;

    paint_live();
}

/* ================= Boot ================= */
static const char * const BOOT_STEPS[] = {
    "initializing display...",
    "calibrating field of view...",
    "loading G3 OS...",
    "ready",
};

static void boot_timer_cb(lv_timer_t * t)
{
    g3.boot_step++;
    if (g3.boot_step < 4) {
        lv_label_set_text(g3.boot_status, BOOT_STEPS[g3.boot_step]);
        return;
    }
    lv_timer_delete(t);
    g3.boot_timer = NULL;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, g3.boot);
    lv_anim_set_exec_cb(&a, anim_opa_cb);
    lv_anim_set_values(&a, 255, 0);
    lv_anim_set_duration(&a, 600);
    lv_anim_set_completed_cb(&a, anim_del_ready_cb);
    lv_anim_start(&a);
    g3.boot = NULL;
}

static void build_boot(void)
{
    lv_obj_t * boot = lv_obj_create(lv_layer_top());
    int32_t    i;

    lv_obj_set_size(boot, SCREEN_W, SCREEN_H);
    lv_obj_align(boot, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(boot, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(boot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(boot, 0, 0);
    lv_obj_set_style_radius(boot, 0, 0);
    lv_obj_set_flex_flow(boot, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(boot, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(boot, 20, 0);
    lv_obj_remove_flag(boot, LV_OBJ_FLAG_SCROLLABLE);
    g3.boot = boot;

    lv_obj_t * mark = g3_plain(boot);
    lv_obj_set_size(mark, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(mark, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(mark, 10, 0);
    for (i = 0; i < 2; i++) {
        lv_obj_t * ring = lv_obj_create(mark);
        lv_obj_set_size(ring, 40, 40);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(ring, lv_color_hex(G_GREEN), 0);
        lv_obj_set_style_border_opa(ring, OP_BRIGHT, 0);
        lv_obj_set_style_border_width(ring, 3, 0);
        lv_obj_remove_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    }

    lv_obj_t * title = g3_label(boot, "EVEN  G3", FONT_LG, OP_BRIGHT);
    lv_obj_set_style_text_letter_space(title, 6, 0);

    lv_obj_t * fill = g3_bar(boot, 0);
    lv_obj_set_width(lv_obj_get_parent(fill), 180);

    g3.boot_status = g3_label(boot, BOOT_STEPS[0], FONT_SM, OP_DIM);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, fill);
    lv_anim_set_exec_cb(&a, anim_width_pct_cb);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_duration(&a, 1700);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    g3.boot_step = 0;
    g3.boot_timer = lv_timer_create(boot_timer_cb, 480, NULL);
}

/* ================= World background / vignette / scanlines ================= */

/* The dimmed "real world" behind the glasses: a radial glow at top-center
 * fading to near-black, matching the HTML #world gradient. */
static void build_world(lv_obj_t * root)
{
    static lv_grad_dsc_t grad;
    lv_obj_t *           world = lv_obj_create(root);

    lv_memzero(&grad, sizeof(grad));
    lv_grad_radial_init(&grad, LV_GRAD_CENTER, lv_pct(10), LV_GRAD_CENTER, lv_pct(120),
                        LV_GRAD_EXTEND_PAD);
    grad.stops_count    = 4;
    grad.stops[0].color = lv_color_hex(0x243444);   grad.stops[0].opa = LV_OPA_COVER; grad.stops[0].frac = 0;
    grad.stops[1].color = lv_color_hex(0x18232f);   grad.stops[1].opa = LV_OPA_COVER; grad.stops[1].frac = 97;
    grad.stops[2].color = lv_color_hex(0x0d141c);   grad.stops[2].opa = LV_OPA_COVER; grad.stops[2].frac = 179;
    grad.stops[3].color = lv_color_hex(0x05090e);   grad.stops[3].opa = LV_OPA_COVER; grad.stops[3].frac = 255;

    lv_obj_set_size(world, SCREEN_W, SCREEN_H);
    lv_obj_align(world, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_border_width(world, 0, 0);
    lv_obj_set_style_radius(world, 0, 0);
    lv_obj_set_style_pad_all(world, 0, 0);
    lv_obj_set_style_bg_opa(world, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad(world, &grad, 0);
    lv_obj_remove_flag(world, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(world, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_background(world);   /* keep behind the carousel */
}

/* Lens vignette: a single radial gradient, transparent center -> dark frame
 * (matches the HTML #vignette). */
static void build_vignette(lv_obj_t * layer)
{
    static lv_grad_dsc_t grad;
    lv_obj_t *           v = lv_obj_create(layer);

    lv_memzero(&grad, sizeof(grad));
    /* Circular radial gradient: radius = distance(center -> "to"). Aim "to" at
     * the corner (100%,100%) so the darkest stop lands on the four corners
     * (120% was too large, leaving the corners un-shadowed). */
    lv_grad_radial_init(&grad, LV_GRAD_CENTER, LV_GRAD_CENTER, lv_pct(100), lv_pct(100),
                        LV_GRAD_EXTEND_PAD);
    grad.stops_count    = 3;
    grad.stops[0].color = lv_color_hex(0x000000); grad.stops[0].opa = LV_OPA_TRANSP; grad.stops[0].frac = 118;
    grad.stops[1].color = lv_color_hex(0x000000); grad.stops[1].opa = 140;           grad.stops[1].frac = 209;
    grad.stops[2].color = lv_color_hex(0x000000); grad.stops[2].opa = 235;           grad.stops[2].frac = 255;

    lv_obj_set_size(v, SCREEN_W, SCREEN_H);
    lv_obj_align(v, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_border_width(v, 0, 0);
    lv_obj_set_style_radius(v, 0, 0);
    lv_obj_set_style_pad_all(v, 0, 0);
    lv_obj_set_style_bg_opa(v, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(v, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_grad(v, &grad, 0);
    lv_obj_remove_flag(v, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(v, LV_OBJ_FLAG_CLICKABLE);
}

/* Draws faint horizontal CRT scanlines across the overlay object. */
static void scanline_draw_cb(lv_event_t * e)
{
    lv_layer_t *      layer = lv_event_get_layer(e);
    lv_obj_t *        obj   = lv_event_get_target_obj(e);
    lv_area_t         coords;
    lv_draw_rect_dsc_t d;
    int32_t           y;

    lv_obj_get_coords(obj, &coords);

    lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(0x000000);
    d.bg_opa   = 12;   /* very subtle micro-LED texture */

    for (y = coords.y1; y <= coords.y2; y += 3) {
        lv_area_t a;
        a.x1 = coords.x1;
        a.x2 = coords.x2;
        a.y1 = y;
        a.y2 = y;       /* 1px tall line, 2px gap */
        lv_draw_rect(layer, &d, &a);
    }
}

static void build_scanlines(lv_obj_t * layer)
{
    lv_obj_t * sl = lv_obj_create(layer);

    lv_obj_set_size(sl, SCREEN_W, SCREEN_H);
    lv_obj_align(sl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(sl, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sl, 0, 0);
    lv_obj_set_style_radius(sl, 0, 0);
    lv_obj_set_style_pad_all(sl, 0, 0);
    lv_obj_remove_flag(sl, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(sl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sl, scanline_draw_cb, LV_EVENT_DRAW_POST, NULL);
}

/* Try to load the SimSun 16 CJK binary font from a few candidate locations. */
static void load_cjk_font(void)
{
    static const char * const candidates[] = {
        "C:D:/work/lv_port_pc_visual_studio/LvglPlatform/lvgl/examples/assets/font/lv_font_simsun_16_cjk.fnt",
        "C:LvglPlatform/lvgl/examples/assets/font/lv_font_simsun_16_cjk.fnt",
        "C:../LvglPlatform/lvgl/examples/assets/font/lv_font_simsun_16_cjk.fnt",
        "C:../../LvglPlatform/lvgl/examples/assets/font/lv_font_simsun_16_cjk.fnt",
        "C:../../../../LvglPlatform/lvgl/examples/assets/font/lv_font_simsun_16_cjk.fnt",
    };
    size_t i;

    g3.font_cjk = NULL;
    for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        g3.font_cjk = lv_binfont_create(candidates[i]);
        if (g3.font_cjk != NULL) {
            LV_LOG_USER("[G3-OS] CJK font loaded: %s", candidates[i]);
            return;
        }
    }
    LV_LOG_WARN("[G3-OS] CJK font not found; falling back to pinyin captions");
}

/* ================= Init ================= */
void demo_g3_os_init(void)
{
    memset(&g3, 0, sizeof(g3));

    /* live data seeds */
    g3.music_idx    = 0;
    g3.music_t      = 168;
    g3.steps        = 6240;
    g3.goal         = 10000;
    g3.nav_eta      = 840;
    g3.nav_dist_x10 = 32;
    g3.nav_instr    = 0;
    g3.cal_mins     = 22;
    g3.trans_i      = 0;
    g3.msg_unread   = 2;
    g3.msg_age      = 132;
    g3.sec          = 9U * 3600U + 53U * 60U; /* 09:53 */
    g3.index        = START_INDEX;
    g3.overlay      = 0;

    seed_chats();

    /* Load the Chinese font before building the screens that use it. */
    load_cjk_font();
    hover_trans_init();

    lv_obj_t * root = lv_screen_active();
    lv_obj_set_style_bg_color(root, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    /* Pointer input: click + drag (follows the finger, snaps on release). */
    lv_obj_add_event_cb(root, root_input_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(root, root_input_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(root, root_input_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(root, root_input_cb, LV_EVENT_PRESS_LOST, NULL);

    /* World backdrop, then the CRT overlays that dim it. These sit BELOW the
     * HUD content (like the HTML z-order: world < vignette < scanlines < HUD),
     * so the tiles/text stay crisp while the "world" around the frame is dimmed. */
    build_world(root);
    build_vignette(root);
    build_scanlines(root);

    /* carousel window + track */
    g3.carousel = g3_plain(root);
    lv_obj_set_size(g3.carousel, SCREEN_W, SCREEN_H);
    lv_obj_align(g3.carousel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(g3.carousel, LV_OBJ_FLAG_CLICKABLE);

    g3.track = g3_plain(g3.carousel);
    lv_obj_set_size(g3.track, SCREEN_W * SCREEN_COUNT, SCREEN_H);
    lv_obj_set_pos(g3.track, -START_INDEX * SCREEN_W, 0);
    lv_obj_set_flex_flow(g3.track, LV_FLEX_FLOW_ROW);
    lv_obj_add_flag(g3.track, LV_OBJ_FLAG_CLICKABLE);

    build_screen_control();
    build_screen_dashboard();
    build_screen_desk1();
    build_screen_desk2();

    build_overlay_notify();
    build_overlay_apps();

    g3.expanded_layer = g3_plain(root);
    lv_obj_set_size(g3.expanded_layer, SCREEN_W, SCREEN_H);
    lv_obj_align(g3.expanded_layer, LV_ALIGN_CENTER, 0, 0);
    /* g3_plain objects are clickable by default; the expanded layer must stay
     * transparent to input at rest, otherwise this full-screen topmost layer
     * swallows all hover/click events meant for the tiles below. It is made
     * clickable only while a tile is expanded (see g3_expand/g3_collapse). */
    lv_obj_remove_flag(g3.expanded_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g3.expanded_layer, exp_layer_click_cb, LV_EVENT_CLICKED, NULL);

    build_chrome();
    update_chrome();
    paint_live();

    g3.live_timer = lv_timer_create(live_tick, 1000, NULL);
    build_boot();

    LV_LOG_USER("[G3-OS] init");
}
