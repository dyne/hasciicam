#include "capture_size.h"

static int abs_i(int v) {
    return v < 0 ? -v : v;
}

int capture_size_score(int target_w, int target_h, int candidate_w, int candidate_h) {
    int delta_w;
    int delta_h;
    int area_delta;
    int aspect_delta;
    int target_area;
    int candidate_area;
    if (target_w <= 0 || target_h <= 0 || candidate_w <= 0 || candidate_h <= 0)
        return 0x7fffffff;

    delta_w = abs_i(target_w - candidate_w);
    delta_h = abs_i(target_h - candidate_h);
    target_area = target_w * target_h;
    candidate_area = candidate_w * candidate_h;
    area_delta = abs_i(target_area - candidate_area);
    aspect_delta = abs_i((target_w * candidate_h) - (candidate_w * target_h));
    return (delta_w * 4) + (delta_h * 4) + (area_delta / 2048) + (aspect_delta / 512);
}

int capture_size_is_better(int target_w,
                           int target_h,
                           int current_w,
                           int current_h,
                           int next_w,
                           int next_h) {
    int current_score = capture_size_score(target_w, target_h, current_w, current_h);
    int next_score = capture_size_score(target_w, target_h, next_w, next_h);

    if (next_score < current_score)
        return 1;
    if (next_score > current_score)
        return 0;

    /* Tie-breaker: choose smaller area to reduce capture cost. */
    return (next_w * next_h) < (current_w * current_h);
}
