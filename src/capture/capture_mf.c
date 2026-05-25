#include "capture_mf.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#endif

struct capture_device {
    int initialized;
};

static int mf_open(capture_device **out, const capture_request *req) {
    capture_device *dev;
    (void)req;

#if defined(_WIN32)
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(hr)) {
        fprintf(stderr, "!! media foundation startup failed (0x%08lx)\n", (unsigned long)hr);
        return 0;
    }
#endif

    dev = calloc(1, sizeof(*dev));
    if (dev == NULL) {
#if defined(_WIN32)
        MFShutdown();
#endif
        return 0;
    }
    dev->initialized = 1;
    *out = dev;

    fprintf(stderr, "!! media foundation backend skeleton is active; capture path not implemented yet\n");
    return 0;
}

static int mf_describe(capture_device *dev, capture_info *info) {
    (void)dev;
    (void)info;
    return 0;
}

static int mf_start(capture_device *dev) {
    (void)dev;
    return 0;
}

static int mf_read(capture_device *dev, capture_frame *frame) {
    (void)dev;
    (void)frame;
    return 0;
}

static void mf_release(capture_device *dev, capture_frame *frame) {
    (void)dev;
    (void)frame;
}

static void mf_stop(capture_device *dev) {
    (void)dev;
}

static void mf_close(capture_device *dev) {
    if (dev == NULL)
        return;
#if defined(_WIN32)
    if (dev->initialized)
        MFShutdown();
#endif
    free(dev);
}

static const char *mf_name(void) {
    return "media-foundation";
}

static const capture_ops ops = {
    mf_open,
    mf_describe,
    mf_start,
    mf_read,
    mf_release,
    mf_stop,
    mf_close,
    mf_name
};

const capture_ops *capture_mf_ops(void) {
    return &ops;
}
