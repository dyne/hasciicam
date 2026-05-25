#include "capture_dshow.h"

#include <stdio.h>

struct capture_device {
    int unused;
};

static int dshow_open(capture_device **out, const capture_request *req) {
    (void)out;
    (void)req;
    fprintf(stderr, "!! directshow backend not implemented yet\n");
    return 0;
}

static int dshow_describe(capture_device *dev, capture_info *info) {
    (void)dev;
    (void)info;
    return 0;
}

static int dshow_start(capture_device *dev) {
    (void)dev;
    return 0;
}

static int dshow_read(capture_device *dev, capture_frame *frame) {
    (void)dev;
    (void)frame;
    return 0;
}

static void dshow_release(capture_device *dev, capture_frame *frame) {
    (void)dev;
    (void)frame;
}

static void dshow_stop(capture_device *dev) {
    (void)dev;
}

static void dshow_close(capture_device *dev) {
    (void)dev;
}

static const char *dshow_name(void) {
    return "directshow";
}

static const capture_ops ops = {
    dshow_open,
    dshow_describe,
    dshow_start,
    dshow_read,
    dshow_release,
    dshow_stop,
    dshow_close,
    dshow_name
};

const capture_ops *capture_dshow_ops(void) {
    return &ops;
}
