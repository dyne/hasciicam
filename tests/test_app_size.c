#include <stdio.h>
#include <string.h>

#include "../src/app/app_size.h"

/* hasciicam modes */
#define LIVE 0
#define HTML 1

static int test_metrics_defaults(void) {
    hasciicam_size_metrics m;
    memset(&m, 0, sizeof(m));
    hasciicam_size_metrics_init(&m);
    return m.capture_pixels_per_char_x == 4 && m.capture_pixels_per_char_y == 8;
}

static int test_pixel_size_plan(void) {
    hasciicam_config cfg;
    hasciicam_size_metrics m;
    hasciicam_size_plan plan;
    memset(&cfg, 0, sizeof(cfg));
    memset(&plan, 0, sizeof(plan));
    hasciicam_size_metrics_init(&m);
    cfg.mode = LIVE;
    cfg.size_intent = HASCIICAM_SIZE_PIXELS;
    cfg.size_w = 1920;
    cfg.size_h = 1200;
    hasciicam_size_build_plan(&cfg, &m, &plan);
    return plan.requested_capture_width == 960 &&
           plan.requested_capture_height == 600 &&
           plan.preferred_ascii_width == 240 &&
           plan.preferred_ascii_height == 75;
}

static int test_char_size_plan(void) {
    hasciicam_config cfg;
    hasciicam_size_metrics m;
    hasciicam_size_plan plan;
    memset(&cfg, 0, sizeof(cfg));
    memset(&plan, 0, sizeof(plan));
    hasciicam_size_metrics_init(&m);
    cfg.mode = HTML;
    cfg.size_intent = HASCIICAM_SIZE_CHARS;
    cfg.size_w = 80;
    cfg.size_h = 25;
    hasciicam_size_build_plan(&cfg, &m, &plan);
    return plan.requested_capture_width == 320 &&
           plan.requested_capture_height == 200 &&
           plan.preferred_ascii_width == 80 &&
           plan.preferred_ascii_height == 25;
}

static int test_compute_ascii_from_capture(void) {
    hasciicam_size_metrics m;
    int aw = 0;
    int ah = 0;
    hasciicam_size_metrics_init(&m);
    hasciicam_size_compute_ascii_from_capture(&m, 1280, 720, &aw, &ah);
    return aw == 320 && ah == 90;
}

static int test_default_live_size_plan_hd(void) {
    hasciicam_size_metrics m;
    hasciicam_size_plan plan;
    hasciicam_size_metrics_init(&m);
    memset(&plan, 0, sizeof(plan));
    hasciicam_size_build_default_live_plan(&m, 1920, 1080, &plan);
    return plan.preferred_ascii_width == 216 &&
           plan.preferred_ascii_height == 60 &&
           plan.requested_capture_width == 864 &&
           plan.requested_capture_height == 480;
}

static int test_default_live_size_plan_small_display(void) {
    hasciicam_size_metrics m;
    hasciicam_size_plan plan;
    hasciicam_size_metrics_init(&m);
    memset(&plan, 0, sizeof(plan));
    hasciicam_size_build_default_live_plan(&m, 800, 600, &plan);
    return plan.preferred_ascii_width == 90 &&
           plan.preferred_ascii_height == 33 &&
           plan.requested_capture_width == 360 &&
           plan.requested_capture_height == 264;
}

static int test_pixel_size_plan_respects_font_height(void) {
    hasciicam_config cfg;
    hasciicam_size_metrics m;
    hasciicam_size_plan plan_16;
    hasciicam_size_plan plan_8;
    memset(&cfg, 0, sizeof(cfg));
    memset(&plan_16, 0, sizeof(plan_16));
    memset(&plan_8, 0, sizeof(plan_8));
    hasciicam_size_metrics_init(&m);
    cfg.mode = LIVE;
    cfg.size_intent = HASCIICAM_SIZE_PIXELS;
    cfg.size_w = 1280;
    cfg.size_h = 720;
    hasciicam_size_build_plan(&cfg, &m, &plan_16);
    m.display_pixels_per_char_y = 8;
    hasciicam_size_build_plan(&cfg, &m, &plan_8);
    return plan_8.preferred_ascii_height > plan_16.preferred_ascii_height &&
           plan_8.requested_capture_height > plan_16.requested_capture_height;
}

static int test_resolve_ascii_prefers_explicit_plan(void) {
    hasciicam_size_metrics m;
    hasciicam_size_plan plan;
    int aw = 0;
    int ah = 0;
    hasciicam_size_metrics_init(&m);
    memset(&plan, 0, sizeof(plan));
    plan.preferred_ascii_width = 80;
    plan.preferred_ascii_height = 30;
    hasciicam_size_resolve_ascii(&m, &plan, 1, 1280, 520, &aw, &ah);
    return aw == 80 && ah == 30;
}

static int test_resolve_ascii_uses_replacement_source(void) {
    hasciicam_size_metrics m;
    hasciicam_size_plan plan;
    int aw = 0;
    int ah = 0;
    hasciicam_size_metrics_init(&m);
    memset(&plan, 0, sizeof(plan));
    plan.preferred_ascii_width = 80;
    plan.preferred_ascii_height = 30;
    hasciicam_size_resolve_ascii(&m, &plan, 0, 800, 1200, &aw, &ah);
    return aw == 200 && ah == 150;
}

static int test_fit_ascii_preserves_portrait_aspect(void) {
    hasciicam_size_metrics m;
    int aw = 270;
    int ah = 240;
    hasciicam_size_metrics_init(&m);
    hasciicam_size_fit_ascii_to_display(&m, 1920, 1080, &aw, &ah);
    return aw == 75 && ah == 67 && aw * m.display_pixels_per_char_x <= 1920 &&
           ah * m.display_pixels_per_char_y <= 1080;
}

static int test_fit_ascii_leaves_small_grid_unchanged(void) {
    hasciicam_size_metrics m;
    int aw = 80;
    int ah = 30;
    hasciicam_size_metrics_init(&m);
    hasciicam_size_fit_ascii_to_display(&m, 1920, 1080, &aw, &ah);
    return aw == 80 && ah == 30;
}

int main(void) {
    if (!test_metrics_defaults()) return 1;
    if (!test_pixel_size_plan()) return 1;
    if (!test_char_size_plan()) return 1;
    if (!test_compute_ascii_from_capture()) return 1;
    if (!test_default_live_size_plan_hd()) return 1;
    if (!test_default_live_size_plan_small_display()) return 1;
    if (!test_pixel_size_plan_respects_font_height()) return 1;
    if (!test_resolve_ascii_prefers_explicit_plan()) return 1;
    if (!test_resolve_ascii_uses_replacement_source()) return 1;
    if (!test_fit_ascii_preserves_portrait_aspect()) return 1;
    if (!test_fit_ascii_leaves_small_grid_unchanged()) return 1;
    printf("app_size tests passed\n");
    return 0;
}
