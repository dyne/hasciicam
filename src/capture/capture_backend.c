#include "capture_backend.h"
#if defined(HASCIICAM_ENABLE_CAPTURE_AVFOUNDATION)
#include "capture_avfoundation.h"
#endif
#if defined(HASCIICAM_ENABLE_CAPTURE_DSHOW)
#include "capture_dshow.h"
#endif
#include "capture_external.h"
#include "capture_synthetic.h"
#if defined(HASCIICAM_ENABLE_CAPTURE_MF)
#include "capture_mf.h"
#endif
#if defined(HASCIICAM_ENABLE_CAPTURE_V4L2)
#include "capture_v4l2.h"
#endif
#include <stdio.h>
#include <string.h>

int quiet = 0;
static char g_capture_last_error[160];

int hasciicam_capture_is_quiet(void) {
    return quiet;
}

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
#if defined(HASCIICAM_ENABLE_CAPTURE_MF)
    return capture_mf_ops();
#elif defined(HASCIICAM_ENABLE_CAPTURE_DSHOW)
    return capture_dshow_ops();
#elif defined(HASCIICAM_ENABLE_CAPTURE_V4L2)
    return capture_v4l2_ops();
#else
    return capture_external_ops();
#endif
#elif defined(__APPLE__)
#if defined(HASCIICAM_ENABLE_CAPTURE_AVFOUNDATION)
    return capture_avfoundation_ops();
#else
    return capture_external_ops();
#endif
#else
#if defined(HASCIICAM_ENABLE_CAPTURE_V4L2)
    return capture_v4l2_ops();
#else
    return capture_external_ops();
#endif
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

    if (req != NULL && req->device != NULL &&
        strcmp(req->device, "synthetic://") == 0) {
        const capture_ops *synthetic_ops = capture_synthetic_ops();
        if (synthetic_ops->open(out_dev, req)) {
            *out_ops = synthetic_ops;
            if (!quiet)
                fprintf(stderr, "Capture backend: %s\n", synthetic_ops->name());
            return 1;
        }
        capture_set_last_error("open failed in backend: synthetic");
        return 0;
    }

    if (req != NULL && req->device != NULL &&
        strcmp(req->device, "external://") == 0) {
        const capture_ops *external_ops = capture_external_ops();
        if (external_ops->open(out_dev, req)) {
            *out_ops = external_ops;
            if (!quiet)
                fprintf(stderr, "Capture backend: %s\n", external_ops->name());
            return 1;
        }
        capture_set_last_error("open failed in backend: external");
        return 0;
    }

#if defined(_WIN32)
#if defined(HASCIICAM_ENABLE_CAPTURE_MF)
    ops_try[0] = capture_mf_ops();
#else
    ops_try[0] = NULL;
#endif
#if defined(HASCIICAM_ENABLE_CAPTURE_DSHOW)
    ops_try[1] = capture_dshow_ops();
#else
    ops_try[1] = NULL;
#endif
#if defined(HASCIICAM_ENABLE_CAPTURE_V4L2)
    ops_try[2] = capture_v4l2_ops();
#else
    ops_try[2] = NULL;
#endif
#elif defined(__APPLE__)
#if defined(HASCIICAM_ENABLE_CAPTURE_AVFOUNDATION)
    ops_try[0] = capture_avfoundation_ops();
#else
    ops_try[0] = NULL;
#endif
    ops_try[1] = NULL;
    ops_try[2] = NULL;
#else
#if defined(HASCIICAM_ENABLE_CAPTURE_V4L2)
    ops_try[0] = capture_v4l2_ops();
#else
    ops_try[0] = NULL;
#endif
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
