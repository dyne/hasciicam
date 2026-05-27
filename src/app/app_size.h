#ifndef HASCIICAM_APP_SIZE_H
#define HASCIICAM_APP_SIZE_H

#include "app_config.h"

typedef struct hasciicam_size_metrics {
    int capture_pixels_per_char_x;
    int capture_pixels_per_char_y;
    int display_pixels_per_char_x;
    int display_pixels_per_char_y;
} hasciicam_size_metrics;

typedef struct hasciicam_size_plan {
    int requested_capture_width;
    int requested_capture_height;
    int preferred_ascii_width;
    int preferred_ascii_height;
} hasciicam_size_plan;

void hasciicam_size_metrics_init(hasciicam_size_metrics *metrics);
void hasciicam_size_build_plan(const hasciicam_config *cfg,
                               const hasciicam_size_metrics *metrics,
                               hasciicam_size_plan *plan);
void hasciicam_size_compute_ascii_from_capture(const hasciicam_size_metrics *metrics,
                                               int capture_width,
                                               int capture_height,
                                               int *ascii_width,
                                               int *ascii_height);

#endif
