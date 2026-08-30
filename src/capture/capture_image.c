#include "capture_image.h"
#include "image_decode.h"
#include <stdlib.h>
#include <string.h>

struct capture_device { image_decode_result image; int running; };
static int image_open(capture_device **out, const capture_request *req) {
    capture_device *dev;
    if (out == NULL || req == NULL || req->image_path == NULL || req->image_path[0] == '\0') return 0;
    dev = calloc(1, sizeof(*dev));
    if (dev == NULL) return 0;
    image_decode_init(&dev->image);
    if (!image_decode_rgb24(req->image_path, &dev->image)) { free(dev); return 0; }
    *out = dev; return 1;
}
static int image_describe(capture_device *dev, capture_info *info) { if (!dev || !info) return 0; info->width=dev->image.width; info->height=dev->image.height; info->stride_bytes=dev->image.stride_bytes; info->pixel_format=CAPTURE_PIXFMT_RGB24; return 1; }
static int image_start(capture_device *dev) { if (!dev) return 0; dev->running=1; return 1; }
static int image_read(capture_device *dev, capture_frame *frame) { if (!dev || !frame || !dev->running) return 0; memset(frame,0,sizeof(*frame)); frame->data=dev->image.pixels; frame->data_size=dev->image.size; frame->width=dev->image.width; frame->height=dev->image.height; frame->stride_bytes=dev->image.stride_bytes; frame->pixel_format=CAPTURE_PIXFMT_RGB24; return 1; }
static void image_release(capture_device *dev, capture_frame *frame) { (void)dev; (void)frame; }
static void image_stop(capture_device *dev) { if (dev) dev->running=0; }
static void image_close(capture_device *dev) { if (!dev) return; image_decode_release(&dev->image); free(dev); }
static const char *image_name(void) { return "image"; }
static const capture_ops ops = { image_open,image_describe,image_start,image_read,image_release,image_stop,image_close,image_name,NULL,NULL,NULL };
const capture_ops *capture_image_ops(void) { return &ops; }
