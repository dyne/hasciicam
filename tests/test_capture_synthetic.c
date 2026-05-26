#include "../src/capture/capture_backend.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    capture_request req;
    capture_device *dev = NULL;
    const capture_ops *ops = NULL;
    capture_info info;
    capture_frame first;
    capture_frame second;
    unsigned char first_pixel;

    memset(&req, 0, sizeof(req));
    memset(&info, 0, sizeof(info));
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));

    req.device = "synthetic://";
    req.requested_width = 64;
    req.requested_height = 32;

    if (!capture_open_default(&req, &dev, &ops) || dev == NULL || ops == NULL) {
        fprintf(stderr, "synthetic open failed\n");
        return 1;
    }
    if (strcmp(ops->name(), "synthetic") != 0) {
        fprintf(stderr, "wrong backend selected: %s\n", ops->name());
        ops->close(dev);
        return 2;
    }
    if (!ops->describe(dev, &info) || info.pixel_format != CAPTURE_PIXFMT_GRAY8) {
        fprintf(stderr, "synthetic describe failed\n");
        ops->close(dev);
        return 3;
    }
    if (!ops->start(dev)) {
        fprintf(stderr, "synthetic start failed\n");
        ops->close(dev);
        return 4;
    }
    if (!ops->read(dev, &first) || first.data == NULL || first.data_size == 0) {
        fprintf(stderr, "first synthetic read failed\n");
        ops->stop(dev);
        ops->close(dev);
        return 5;
    }
    first_pixel = first.data[0];
    ops->release(dev, &first);

    if (!ops->read(dev, &second) || second.data == NULL || second.data_size == 0) {
        fprintf(stderr, "second synthetic read failed\n");
        ops->stop(dev);
        ops->close(dev);
        return 6;
    }
    if (second.data[0] == first_pixel) {
        fprintf(stderr, "synthetic frames did not change\n");
        ops->release(dev, &second);
        ops->stop(dev);
        ops->close(dev);
        return 7;
    }
    ops->release(dev, &second);

    ops->stop(dev);
    ops->close(dev);
    return 0;
}
