#include "capture_backend.h"
#include <string.h>
#ifndef HASCIICAM_SOURCE_DIR
#define HASCIICAM_SOURCE_DIR "."
#endif
int main(void) {
    capture_request req = {0}; capture_device *dev = 0; const capture_ops *ops = 0; capture_info info = {0}; capture_frame a = {0}, b = {0};
    req.image_path = HASCIICAM_SOURCE_DIR "/tests/fixtures/image-red.png";
    if (!capture_open_default(&req,&dev,&ops) || !ops || strcmp(ops->name(),"image") || !ops->describe(dev,&info) || info.pixel_format != CAPTURE_PIXFMT_RGB24 || ops->read(dev,&a)) return 1;
    if (!ops->start(dev) || !ops->read(dev,&a) || !ops->read(dev,&b) || a.data != b.data || a.data_size != 3) return 2;
    ops->release(dev,&a); ops->stop(dev); if (ops->read(dev,&a)) return 3; ops->close(dev);
    req.image_path=HASCIICAM_SOURCE_DIR "/tests/fixtures/missing.png"; return capture_open_default(&req,&dev,&ops) ? 4 : 0;
}
