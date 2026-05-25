#include "capture_v4l2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32)

int vid_detect(char *devfile) {
    (void)devfile;
    fprintf(stderr, "!! video capture is not implemented on this platform yet\n");
    return -1;
}

int vid_init(void) {
    return -1;
}

void grab_one(void) {
}

void vid_close(void) {
}

#elif defined(__linux__)

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/videodev2.h>

#include <aalib.h>

extern int inputch;
extern int whchanged;
extern int user_w;
extern int user_h;

extern struct geometry vid_geo;
extern unsigned char *grey;
extern int YtoRGB[256];
extern int xstep;
extern int ystep;
extern int xbytestep;
extern int ybytestep;
extern int renderhop;
extern int framenum;
extern int gw;
extern int gh;
extern int vw;
extern int vh;
extern int aw;
extern int ah;
extern size_t greysize;
extern int vbytesperline;

extern aa_context *ascii_context;
void YUV422_to_grey_scaled(unsigned char *src, unsigned char *dst,
                           int src_w, int src_h, int dst_w, int dst_h);

static int buftype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
static struct v4l2_capability capability;
static struct v4l2_input input;
static struct v4l2_standard standard;
static struct v4l2_format format;
static struct v4l2_requestbuffers reqbuf;
static struct v4l2_buffer buffer;
static struct {
    void *start;
    size_t length;
} *buffers;
static int fd = -1;

int vid_detect(char *devfile) {
    unsigned int i;

    if (-1 == (fd = open(devfile, O_RDWR | O_NONBLOCK))) {
        perror("!! error in opening video capture device: ");
        return -1;
    } else {
        close(fd);
        fd = open(devfile, O_RDWR);
    }

    memset(&capability, 0, sizeof(capability));
    if(-1 == ioctl(fd, VIDIOC_QUERYCAP, &capability)) {
        perror("VIDIOC_QUERYCAP");
        exit(EXIT_FAILURE);
    }
    if((capability.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0){
        printf("Fatal: Device %s does not support video capture\n", capability.card);
        exit(EXIT_FAILURE);
    }
    if((capability.capabilities & V4L2_CAP_STREAMING) == 0){
        printf("Fatal: Device %s does not support streaming data capture\n", capability.card);
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Device detected is %s\n", devfile);
    fprintf(stderr, "Card name: %s\n", capability.card);

    if(-1 == ioctl(fd, VIDIOC_S_INPUT, &inputch)) {
        perror("VIDIOC_S_INPUT");
        exit(EXIT_FAILURE);
    }

    memset(&input, 0, sizeof(input));
    input.index = inputch;
    if(-1 == ioctl(fd, VIDIOC_ENUMINPUT, &input)) {
        perror("VIDIOC_ENUMINPUT");
        exit(EXIT_FAILURE);
    }
    printf("Current input is %s\n", input.name);

    memset(&standard, 0, sizeof(standard));
    standard.index = 0;
    while(0 == ioctl(fd, VIDIOC_ENUMSTD, &standard)) {
        if(standard.id & input.std)
            printf("   - %s\n", standard.name);
        standard.index++;
    }

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == ioctl(fd, VIDIOC_G_FMT, &format)) {
        perror("VIDIOC_G_FMT");
        exit(EXIT_FAILURE);
    }

    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    if (whchanged == 1) {
        fprintf(stderr, "user defined size: %u x %u\n", user_w, user_h);
        format.fmt.pix.width  = user_w;
        format.fmt.pix.height = user_h;
    }

    if(-1 == ioctl(fd, VIDIOC_S_FMT, &format)) {
        perror("VIDIOC_S_FMT");
        exit(EXIT_FAILURE);
    }

    printf("Current capture is %u x %u\n",
           format.fmt.pix.width, format.fmt.pix.height);
    printf("format %4.4s, %u bytes-per-line\n",
           (char*)&format.fmt.pix.pixelformat,
           format.fmt.pix.bytesperline);

    return 1;
}

int vid_init(void) {
    int i, j;

    vw = format.fmt.pix.width;
    vh = format.fmt.pix.height;
    vbytesperline = format.fmt.pix.bytesperline;

    vid_geo.w = vw;
    vid_geo.h = vh;
    vid_geo.size = vw * vh;

    xbytestep = xstep + xstep;
    ybytestep = vbytesperline * (ystep - 1);
    gw = vw / xstep;
    gh = vh / ystep;
    aw = gw / 2;
    ah = gh / 2;

    greysize = gw * gh;
    grey = malloc(greysize);
    if (grey == NULL) {
        printf("calloc failure!");
        exit(EXIT_FAILURE);
    }

    printf("Grey buffer is %i bytes\n", (int)greysize);
    for (j = 0; j < 256; ++j)
        YtoRGB[j] = (int)(1.164 * (j - 256));

    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = 32;

    if (-1 == ioctl(fd, VIDIOC_REQBUFS, &reqbuf)) {
        if (errno == EINVAL)
            printf("Fatal: Video capturing by mmap-streaming is not supported\n");
        else
            perror("VIDIOC_REQBUFS");
        exit(EXIT_FAILURE);
    }

    buffers = calloc(reqbuf.count, sizeof(*buffers));
    if (buffers == NULL) {
        printf("calloc failure!");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < reqbuf.count; i++) {
        memset(&buffer, 0, sizeof(buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if (-1 == ioctl(fd, VIDIOC_QUERYBUF, &buffer)) {
            perror("VIDIOC_QUERYBUF");
            exit(EXIT_FAILURE);
        }

        buffers[i].length = buffer.length;
        buffers[i].start = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
                                MAP_SHARED, fd, buffer.m.offset);

        if (MAP_FAILED == buffers[i].start) {
            perror("mmap");
            exit(EXIT_FAILURE);
        }
    }

    for (i = 0; i < reqbuf.count; i++) {
        memset(&buffer, 0, sizeof(buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if (-1 == ioctl(fd, VIDIOC_QBUF, &buffer)) {
            perror("VIDIOC_QBUF");
            exit(EXIT_FAILURE);
        }
    }

    if(-1 == ioctl(fd, VIDIOC_STREAMON, &buftype)) {
        perror("VIDIOC_STREAMON");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < (int)greysize; i++)
        grey[i] = i % 160;

    return 1;
}

void grab_one(void) {
    if (-1 == ioctl(fd, VIDIOC_DQBUF, &buffer)) {
        perror("VIDIOC_DQBUF");
        exit(EXIT_FAILURE);
    }

    if((++framenum) == renderhop) {
        int ascii_width;
        int ascii_height;
        int grey_size;
        int dest_size;
        int copy_size;

        framenum = 0;
        ascii_width = aa_imgwidth(ascii_context);
        ascii_height = aa_imgheight(ascii_context);

        YUV422_to_grey_scaled(buffers[buffer.index].start, grey, vw, vh,
                              ascii_width, ascii_height);

        grey_size = ascii_width * ascii_height;
        dest_size = aa_imgwidth(ascii_context) * aa_imgheight(ascii_context);
        copy_size = (grey_size < dest_size) ? grey_size : dest_size;
        if (copy_size > 0) {
            memcpy(aa_image(ascii_context), grey, copy_size);
            aa_fastrender(ascii_context, 0, 0, ascii_width / 2, ascii_height / 2);
        }
        aa_flush(ascii_context);
    }

    if (-1 == ioctl(fd, VIDIOC_QBUF, &buffer)) {
        perror("VIDIOC_QBUF");
        exit(EXIT_FAILURE);
    }
}

void vid_close(void) {
    int i;

    if (fd >= 0) {
        if (-1 == ioctl(fd, VIDIOC_STREAMOFF, &buftype))
            perror("VIDIOC_STREAMOFF");
    }

    if (buffers != NULL) {
        for (i = 0; i < reqbuf.count; i++) {
            if (buffers[i].start != NULL && buffers[i].length > 0)
                munmap(buffers[i].start, buffers[i].length);
        }
        free(buffers);
        buffers = NULL;
    }

    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

#else

int vid_detect(char *devfile) {
    (void)devfile;
    fprintf(stderr, "!! video capture is not implemented on this platform yet\n");
    return -1;
}

int vid_init(void) {
    return -1;
}

void grab_one(void) {
}

void vid_close(void) {
}

#endif
