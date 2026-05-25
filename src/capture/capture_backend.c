#include "capture_backend.h"
#include "capture_avfoundation.h"
#include "capture_dshow.h"
#include "capture_mf.h"
#include "capture_v4l2.h"
#include <stdio.h>
#include <string.h>

extern int quiet;
static char g_capture_last_error[160];

static void capture_set_last_error(const char *message) {
    if (message == NULL || message[0] == '\0') {
        g_capture_last_error[0] = '\0';
        return;
    }
    strncpy(g_capture_last_error, message, sizeof(g_capture_last_error) - 1);
    g_capture_last_error[sizeof(g_capture_last_error) - 1] = '\0';
}

const char *capture_last_error(void) {
    if (g_capture_last_error[0] == '\0')
        return "unknown error";
    return g_capture_last_error;
}

const capture_ops *capture_default_ops(void) {
#if defined(_WIN32)
    return capture_mf_ops();
#elif defined(__APPLE__)
    return capture_avfoundation_ops();
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
    capture_set_last_error(NULL);

#if defined(_WIN32)
    ops_try[0] = capture_mf_ops();
    ops_try[1] = capture_dshow_ops();
    ops_try[2] = capture_v4l2_ops();
#elif defined(__APPLE__)
    ops_try[0] = capture_avfoundation_ops();
    ops_try[1] = NULL;
    ops_try[2] = NULL;
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
            capture_set_last_error(NULL);
            if (!quiet)
                fprintf(stderr, "Capture backend: %s\n", ops_try[i]->name());
            return 1;
        }
        {
            char message[160];
            snprintf(message, sizeof(message), "open failed in backend: %s",
                     ops_try[i]->name());
            capture_set_last_error(message);
        }
        if (!quiet)
            fprintf(stderr, "Capture backend failed: %s\n", ops_try[i]->name());
    }

    return 0;
}
