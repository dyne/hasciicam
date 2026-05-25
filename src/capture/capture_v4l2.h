#ifndef HASCIICAM_CAPTURE_V4L2_H
#define HASCIICAM_CAPTURE_V4L2_H

struct geometry {
    int w;
    int h;
    int size;
    int bright;
    int contrast;
    int gamma;
};

int vid_detect(char *devfile);
int vid_init(void);
void grab_one(void);
void vid_close(void);

#endif
