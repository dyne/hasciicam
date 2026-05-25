#include "capture_backend.h"
#include "capture_dshow.h"
#include "capture_mf.h"
#include "capture_v4l2.h"
#include <stdio.h>

extern int quiet;

const capture_ops *capture_default_ops(void) {
#if defined(_WIN32)
    return capture_mf_ops();
#else
    return capture_v4l2_ops();
#endif
}

int capture_open_default(const capture_request *req,
                         capture_device **out_dev,
                         const capture_ops **out_ops) {
    const capture_ops *ops_try[3];
    int i;

    if (out_dev == NULL || out_ops == NULL)
        return 0;

#if defined(_WIN32)
    ops_try[0] = capture_mf_ops();
    ops_try[1] = capture_dshow_ops();
    ops_try[2] = capture_v4l2_ops();
#else
    ops_try[0] = capture_v4l2_ops();
    ops_try[1] = NULL;
    ops_try[2] = NULL;
#endif

    for (i = 0; i < 3; i++) {
        if (ops_try[i] == NULL)
            continue;
        if (ops_try[i]->open(out_dev, req)) {
            *out_ops = ops_try[i];
            if (!quiet)
                fprintf(stderr, "Capture backend: %s\n", ops_try[i]->name());
            return 1;
        }
        if (!quiet)
            fprintf(stderr, "Capture backend failed: %s\n", ops_try[i]->name());
    }

    return 0;
}
