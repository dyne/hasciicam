#include "app_size.h"

void hasciicam_size_metrics_init(hasciicam_size_metrics *metrics) {
    if (metrics == 0)
        return;
    /*
     * Current capture-to-ascii path:
     * capture -> gray downsamples by 2x4 -> aa render halves again.
     * Net effect: one ascii char represents 4x8 source pixels.
     */
    metrics->capture_pixels_per_char_x = 4;
    metrics->capture_pixels_per_char_y = 8;
    /*
     * Current SDL presentation path uses 8x16 pixel character cells.
     * When additional live fonts are introduced, update this mapping.
     */
    metrics->display_pixels_per_char_x = 8;
    metrics->display_pixels_per_char_y = 16;
}

static int positive_or_zero(int value) {
    return value > 0 ? value : 0;
}

static int div_round_nearest(int num, int den) {
    if (den <= 0)
        return 0;
    return (num + (den / 2)) / den;
}

static int clamp_min(int value, int minimum) {
    return value < minimum ? minimum : value;
}

void hasciicam_size_build_plan(const hasciicam_config *cfg,
                               const hasciicam_size_metrics *metrics,
                               hasciicam_size_plan *plan) {
    if (cfg == 0 || metrics == 0 || plan == 0)
        return;

    plan->requested_capture_width = 0;
    plan->requested_capture_height = 0;
    plan->preferred_ascii_width = 0;
    plan->preferred_ascii_height = 0;

    if (cfg->size_w <= 0 || cfg->size_h <= 0 || cfg->size_intent == HASCIICAM_SIZE_NONE)
        return;

    if (cfg->size_intent == HASCIICAM_SIZE_PIXELS) {
        /*
         * Pixel intent targets final output pixel size.
         * Convert to an ascii grid using display cell metrics, then derive
         * required capture target from capture-per-char metrics.
         */
        plan->preferred_ascii_width = positive_or_zero(
            div_round_nearest(cfg->size_w, metrics->display_pixels_per_char_x));
        plan->preferred_ascii_height = positive_or_zero(
            div_round_nearest(cfg->size_h, metrics->display_pixels_per_char_y));
        if (plan->preferred_ascii_width < 1)
            plan->preferred_ascii_width = 1;
        if (plan->preferred_ascii_height < 1)
            plan->preferred_ascii_height = 1;
        plan->requested_capture_width =
            plan->preferred_ascii_width * metrics->capture_pixels_per_char_x;
        plan->requested_capture_height =
            plan->preferred_ascii_height * metrics->capture_pixels_per_char_y;
    } else if (cfg->size_intent == HASCIICAM_SIZE_CHARS) {
        plan->preferred_ascii_width = cfg->size_w;
        plan->preferred_ascii_height = cfg->size_h;
        plan->requested_capture_width = cfg->size_w * metrics->capture_pixels_per_char_x;
        plan->requested_capture_height = cfg->size_h * metrics->capture_pixels_per_char_y;
    }
}

void hasciicam_size_build_default_live_plan(const hasciicam_size_metrics *metrics,
                                            int screen_width,
                                            int screen_height,
                                            hasciicam_size_plan *plan) {
    int target_pixels_w;
    int target_pixels_h;
    int ascii_w;
    int ascii_h;

    if (metrics == 0 || plan == 0)
        return;

    plan->requested_capture_width = 0;
    plan->requested_capture_height = 0;
    plan->preferred_ascii_width = 0;
    plan->preferred_ascii_height = 0;

    if (screen_width <= 0 || screen_height <= 0)
        return;

    target_pixels_w = (screen_width * 9) / 10;
    target_pixels_h = (screen_height * 9) / 10;

    if (screen_width >= 640)
        target_pixels_w = clamp_min(target_pixels_w, 640);
    if (screen_height >= 480)
        target_pixels_h = clamp_min(target_pixels_h, 480);

    if (target_pixels_w > screen_width)
        target_pixels_w = screen_width;
    if (target_pixels_h > screen_height)
        target_pixels_h = screen_height;

    ascii_w = target_pixels_w / metrics->display_pixels_per_char_x;
    ascii_h = target_pixels_h / metrics->display_pixels_per_char_y;
    if (ascii_w < 1)
        ascii_w = 1;
    if (ascii_h < 1)
        ascii_h = 1;

    plan->preferred_ascii_width = ascii_w;
    plan->preferred_ascii_height = ascii_h;
    plan->requested_capture_width = ascii_w * metrics->capture_pixels_per_char_x;
    plan->requested_capture_height = ascii_h * metrics->capture_pixels_per_char_y;
}

void hasciicam_size_compute_ascii_from_capture(const hasciicam_size_metrics *metrics,
                                               int capture_width,
                                               int capture_height,
                                               int *ascii_width,
                                               int *ascii_height) {
    int aw;
    int ah;
    if (metrics == 0 || ascii_width == 0 || ascii_height == 0) {
        return;
    }
    aw = capture_width / metrics->capture_pixels_per_char_x;
    ah = capture_height / metrics->capture_pixels_per_char_y;
    if (aw < 1)
        aw = 1;
    if (ah < 1)
        ah = 1;
    *ascii_width = aw;
    *ascii_height = ah;
}
