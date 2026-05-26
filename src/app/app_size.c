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
}

static int positive_or_zero(int value) {
    return value > 0 ? value : 0;
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
        plan->requested_capture_width = cfg->size_w;
        plan->requested_capture_height = cfg->size_h;
        plan->preferred_ascii_width =
            positive_or_zero(cfg->size_w / metrics->capture_pixels_per_char_x);
        plan->preferred_ascii_height =
            positive_or_zero(cfg->size_h / metrics->capture_pixels_per_char_y);
    } else if (cfg->size_intent == HASCIICAM_SIZE_CHARS) {
        plan->preferred_ascii_width = cfg->size_w;
        plan->preferred_ascii_height = cfg->size_h;
        plan->requested_capture_width = cfg->size_w * metrics->capture_pixels_per_char_x;
        plan->requested_capture_height = cfg->size_h * metrics->capture_pixels_per_char_y;
    }
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
