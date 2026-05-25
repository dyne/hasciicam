#include "capture_v4l2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/videodev2.h>

struct v4l2_buffer_map {
    void *start;
    size_t length;
};

struct capture_device {
    int fd;
    int input_channel;
    int width;
    int height;
    int stride_bytes;
    int buffer_count;
    int active_index;
    int streaming;
    struct v4l2_buffer_map *buffers;
};

struct geometry {
    int w;
    int h;
    int size;
    int bright;
    int contrast;
    int gamma;
};

static int v4l2_open(capture_device **out, const capture_request *req) {
    struct capture_device *dev;
    struct v4l2_capability capability;
    struct v4l2_input input;
    struct v4l2_standard standard;
    struct v4l2_format format;
    int fd;

    fd = open(req->device, O_RDWR | O_NONBLOCK);
    if (fd == -1) {
        perror("!! error in opening video capture device: ");
        return 0;
    }
    close(fd);

    fd = open(req->device, O_RDWR);
    if (fd == -1) {
        perror("!! error in opening video capture device: ");
        return 0;
    }

    memset(&capability, 0, sizeof(capability));
    if (ioctl(fd, VIDIOC_QUERYCAP, &capability) == -1) {
        perror("VIDIOC_QUERYCAP");
        close(fd);
        return 0;
    }
    if ((capability.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
        fprintf(stderr, "Fatal: Device %s does not support video capture\n", capability.card);
        close(fd);
        return 0;
    }
    if ((capability.capabilities & V4L2_CAP_STREAMING) == 0) {
        fprintf(stderr, "Fatal: Device %s does not support streaming data capture\n", capability.card);
        close(fd);
        return 0;
    }

    fprintf(stderr, "Device detected is %s\n", req->device);
    fprintf(stderr, "Card name: %s\n", capability.card);

    if (ioctl(fd, VIDIOC_S_INPUT, (void *)&req->input_channel) == -1) {
        perror("VIDIOC_S_INPUT");
        close(fd);
        return 0;
    }

    memset(&input, 0, sizeof(input));
    input.index = req->input_channel;
    if (ioctl(fd, VIDIOC_ENUMINPUT, &input) == -1) {
        perror("VIDIOC_ENUMINPUT");
        close(fd);
        return 0;
    }
    fprintf(stderr, "Current input is %s\n", input.name);

    memset(&standard, 0, sizeof(standard));
    standard.index = 0;
    while (ioctl(fd, VIDIOC_ENUMSTD, &standard) == 0) {
        if (standard.id & input.std)
            fprintf(stderr, "   - %s\n", standard.name);
        standard.index++;
    }

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &format) == -1) {
        perror("VIDIOC_G_FMT");
        close(fd);
        return 0;
    }

    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    if (req->requested_width > 0 && req->requested_height > 0) {
        format.fmt.pix.width = req->requested_width;
        format.fmt.pix.height = req->requested_height;
        fprintf(stderr, "user defined size: %u x %u\n",
                req->requested_width, req->requested_height);
    }

    if (ioctl(fd, VIDIOC_S_FMT, &format) == -1) {
        perror("VIDIOC_S_FMT");
        close(fd);
        return 0;
    }

    fprintf(stderr, "Current capture is %u x %u\n",
            format.fmt.pix.width, format.fmt.pix.height);
    fprintf(stderr, "format %4.4s, %u bytes-per-line\n",
            (char *)&format.fmt.pix.pixelformat, format.fmt.pix.bytesperline);

    dev = calloc(1, sizeof(*dev));
    if (dev == NULL) {
        close(fd);
        return 0;
    }

    dev->fd = fd;
    dev->input_channel = req->input_channel;
    dev->width = (int)format.fmt.pix.width;
    dev->height = (int)format.fmt.pix.height;
    dev->stride_bytes = (int)format.fmt.pix.bytesperline;
    dev->active_index = -1;
    *out = dev;
    return 1;
}

static int v4l2_describe(capture_device *dev, capture_info *info) {
    if (dev == NULL || info == NULL)
        return 0;
    info->width = dev->width;
    info->height = dev->height;
    info->stride_bytes = dev->stride_bytes;
    info->pixel_format = CAPTURE_PIXFMT_YUYV;
    return 1;
}

static int v4l2_start(capture_device *dev) {
    struct v4l2_requestbuffers reqbuf;
    struct v4l2_buffer buffer;
    int i;
    int buftype;

    if (dev == NULL)
        return 0;

    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = 32;

    if (ioctl(dev->fd, VIDIOC_REQBUFS, &reqbuf) == -1) {
        if (errno == EINVAL)
            fprintf(stderr, "Fatal: Video capturing by mmap-streaming is not supported\n");
        else
            perror("VIDIOC_REQBUFS");
        return 0;
    }

    dev->buffers = calloc(reqbuf.count, sizeof(*dev->buffers));
    if (dev->buffers == NULL)
        return 0;
    dev->buffer_count = (int)reqbuf.count;

    for (i = 0; i < dev->buffer_count; i++) {
        memset(&buffer, 0, sizeof(buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = (unsigned int)i;

        if (ioctl(dev->fd, VIDIOC_QUERYBUF, &buffer) == -1) {
            perror("VIDIOC_QUERYBUF");
            return 0;
        }

        dev->buffers[i].length = buffer.length;
        dev->buffers[i].start = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, dev->fd, buffer.m.offset);
        if (dev->buffers[i].start == MAP_FAILED) {
            perror("mmap");
            return 0;
        }
    }

    for (i = 0; i < dev->buffer_count; i++) {
        memset(&buffer, 0, sizeof(buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = (unsigned int)i;
        if (ioctl(dev->fd, VIDIOC_QBUF, &buffer) == -1) {
            perror("VIDIOC_QBUF");
            return 0;
        }
    }

    buftype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(dev->fd, VIDIOC_STREAMON, &buftype) == -1) {
        perror("VIDIOC_STREAMON");
        return 0;
    }
    dev->streaming = 1;
    return 1;
}

static int v4l2_read(capture_device *dev, capture_frame *frame) {
    struct v4l2_buffer buffer;

    if (dev == NULL || frame == NULL)
        return 0;

    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;

    if (ioctl(dev->fd, VIDIOC_DQBUF, &buffer) == -1) {
        perror("VIDIOC_DQBUF");
        return 0;
    }
    if ((int)buffer.index < 0 || (int)buffer.index >= dev->buffer_count)
        return 0;

    dev->active_index = (int)buffer.index;
    frame->data = dev->buffers[dev->active_index].start;
    frame->data_size = dev->buffers[dev->active_index].length;
    frame->width = dev->width;
    frame->height = dev->height;
    frame->stride_bytes = dev->stride_bytes;
    frame->pixel_format = CAPTURE_PIXFMT_YUYV;
    return 1;
}

static void v4l2_release(capture_device *dev, capture_frame *frame) {
    struct v4l2_buffer buffer;

    if (dev == NULL || dev->active_index < 0)
        return;

    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = (unsigned int)dev->active_index;
    if (ioctl(dev->fd, VIDIOC_QBUF, &buffer) == -1)
        perror("VIDIOC_QBUF");

    dev->active_index = -1;
    if (frame != NULL)
        memset(frame, 0, sizeof(*frame));
}

static void v4l2_stop(capture_device *dev) {
    int buftype;
    int i;

    if (dev == NULL)
        return;

    if (dev->streaming) {
        buftype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(dev->fd, VIDIOC_STREAMOFF, &buftype) == -1)
            perror("VIDIOC_STREAMOFF");
        dev->streaming = 0;
    }

    if (dev->buffers != NULL) {
        for (i = 0; i < dev->buffer_count; i++) {
            if (dev->buffers[i].start != NULL && dev->buffers[i].length > 0)
                munmap(dev->buffers[i].start, dev->buffers[i].length);
        }
        free(dev->buffers);
        dev->buffers = NULL;
        dev->buffer_count = 0;
    }
}

static void v4l2_close(capture_device *dev) {
    if (dev == NULL)
        return;
    v4l2_stop(dev);
    if (dev->fd >= 0)
        close(dev->fd);
    free(dev);
}

static const char *v4l2_name(void) {
    return "v4l2";
}

static const capture_ops ops = {
    v4l2_open,
    v4l2_describe,
    v4l2_start,
    v4l2_read,
    v4l2_release,
    v4l2_stop,
    v4l2_close,
    v4l2_name
};

const capture_ops *capture_v4l2_ops(void) {
    return &ops;
}

#else

struct capture_device {
    int unused;
};

static int unsupported_open(capture_device **out, const capture_request *req) {
    (void)out;
    (void)req;
    fprintf(stderr, "!! video capture is not implemented on this platform yet\n");
    return 0;
}

static int unsupported_describe(capture_device *dev, capture_info *info) {
    (void)dev;
    (void)info;
    return 0;
}

static int unsupported_start(capture_device *dev) {
    (void)dev;
    return 0;
}

static int unsupported_read(capture_device *dev, capture_frame *frame) {
    (void)dev;
    (void)frame;
    return 0;
}

static void unsupported_release(capture_device *dev, capture_frame *frame) {
    (void)dev;
    (void)frame;
}

static void unsupported_stop(capture_device *dev) {
    (void)dev;
}

static void unsupported_close(capture_device *dev) {
    (void)dev;
}

static const char *unsupported_name(void) {
    return "unsupported";
}

static const capture_ops ops = {
    unsupported_open,
    unsupported_describe,
    unsupported_start,
    unsupported_read,
    unsupported_release,
    unsupported_stop,
    unsupported_close,
    unsupported_name
};

const capture_ops *capture_v4l2_ops(void) {
    return &ops;
}

#endif
