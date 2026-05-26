#ifndef HASCIICAM_CAPTURE_SIZE_H
#define HASCIICAM_CAPTURE_SIZE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct capture_size_candidate {
    int width;
    int height;
} capture_size_candidate;

int capture_size_score(int target_w, int target_h, int candidate_w, int candidate_h);
int capture_size_is_better(int target_w,
                           int target_h,
                           int current_w,
                           int current_h,
                           int next_w,
                           int next_h);

#ifdef __cplusplus
}
#endif

#endif
