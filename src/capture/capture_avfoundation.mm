#include "capture_avfoundation.h"

#if defined(__APPLE__)

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <dispatch/dispatch.h>
#import <Foundation/Foundation.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>

@interface HasciicamFrameDelegate : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate> {
@public
    std::mutex frame_mutex;
    std::condition_variable frame_ready;
    std::vector<unsigned char> frame_data;
    int frame_width;
    int frame_height;
    int frame_stride;
    int has_frame;
}
@end

@implementation HasciicamFrameDelegate
- (instancetype)init {
    self = [super init];
    if (self) {
        frame_width = 0;
        frame_height = 0;
        frame_stride = 0;
        has_frame = 0;
    }
    return self;
}

- (void)captureOutput:(AVCaptureOutput *)output
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection {
    CVImageBufferRef image_buffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    CVPixelBufferRef pixel_buffer = (CVPixelBufferRef)image_buffer;
    (void)output;
    (void)connection;
    if (pixel_buffer == NULL)
        return;

    CVPixelBufferLockBaseAddress(pixel_buffer, kCVPixelBufferLock_ReadOnly);
    unsigned char *base = (unsigned char *)CVPixelBufferGetBaseAddress(pixel_buffer);
    int width = (int)CVPixelBufferGetWidth(pixel_buffer);
    int height = (int)CVPixelBufferGetHeight(pixel_buffer);
    int stride = (int)CVPixelBufferGetBytesPerRow(pixel_buffer);
    size_t size = (size_t)stride * (size_t)height;

    {
        std::lock_guard<std::mutex> lock(frame_mutex);
        frame_data.resize(size);
        memcpy(frame_data.data(), base, size);
        frame_width = width;
        frame_height = height;
        frame_stride = stride;
        has_frame = 1;
    }
    frame_ready.notify_one();
    CVPixelBufferUnlockBaseAddress(pixel_buffer, kCVPixelBufferLock_ReadOnly);
}
@end

struct capture_device {
    AVCaptureSession *session;
    AVCaptureDeviceInput *input;
    AVCaptureVideoDataOutput *output;
    dispatch_queue_t queue;
    HasciicamFrameDelegate *delegate;
};

static AVCaptureDevice *find_matching_device(const capture_request *req) {
    NSArray<AVCaptureDevice *> *devices =
        [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
    NSString *matcher = nil;
    if (req != NULL && req->device != NULL && req->device[0] != '\0') {
        matcher = [NSString stringWithUTF8String:req->device];
    }
    for (AVCaptureDevice *device in devices) {
        if (matcher == nil || [matcher length] == 0)
            return device;
        NSRange range = [[device localizedName] rangeOfString:matcher
                                                     options:NSCaseInsensitiveSearch];
        if (range.location != NSNotFound)
            return device;
    }
    return nil;
}

static int avf_open(capture_device **out, const capture_request *req) {
    AVCaptureDevice *device = nil;
    NSError *error = nil;
    capture_device *dev = NULL;

    if (out == NULL)
        return 0;

    device = find_matching_device(req);
    if (device == nil) {
        fprintf(stderr, "!! requested avfoundation device was not found\n");
        return 0;
    }

    dev = (capture_device *)calloc(1, sizeof(*dev));
    if (dev == NULL)
        return 0;

    dev->session = [[AVCaptureSession alloc] init];
    dev->input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
    if (dev->input == nil || error != nil) {
        fprintf(stderr, "!! avfoundation could not create device input\n");
        free(dev);
        return 0;
    }
    if (![dev->session canAddInput:dev->input]) {
        fprintf(stderr, "!! avfoundation cannot add capture input\n");
        free(dev);
        return 0;
    }
    [dev->session addInput:dev->input];

    dev->output = [[AVCaptureVideoDataOutput alloc] init];
    dev->output.videoSettings = @{
        (id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA)
    };
    dev->output.alwaysDiscardsLateVideoFrames = YES;
    if (![dev->session canAddOutput:dev->output]) {
        fprintf(stderr, "!! avfoundation cannot add video output\n");
        free(dev);
        return 0;
    }
    [dev->session addOutput:dev->output];

    dev->delegate = [[HasciicamFrameDelegate alloc] init];
    dev->queue = dispatch_queue_create("org.dyne.hasciicam.avfoundation", DISPATCH_QUEUE_SERIAL);
    [dev->output setSampleBufferDelegate:dev->delegate queue:dev->queue];

    // Seed geometry from activeFormat: describe() runs before start(), so
    // the delegate hasn't received a frame yet and its width/height are 0.
    if (device.activeFormat != nil) {
        CMFormatDescriptionRef desc = device.activeFormat.formatDescription;
        if (desc != NULL) {
            CMVideoDimensions dims = CMVideoFormatDescriptionGetDimensions(desc);
            if (dims.width > 0 && dims.height > 0) {
                std::lock_guard<std::mutex> lock(dev->delegate->frame_mutex);
                dev->delegate->frame_width = dims.width;
                dev->delegate->frame_height = dims.height;
                dev->delegate->frame_stride = dims.width * 4;
            }
        }
    }

    *out = dev;
    return 1;
}

static int avf_describe(capture_device *dev, capture_info *info) {
    if (dev == NULL || info == NULL || dev->delegate == nil)
        return 0;
    {
        std::lock_guard<std::mutex> lock(dev->delegate->frame_mutex);
        info->width = dev->delegate->frame_width;
        info->height = dev->delegate->frame_height;
        info->stride_bytes = dev->delegate->frame_stride;
    }
    info->pixel_format = CAPTURE_PIXFMT_BGRA32;
    return 1;
}

static int avf_start(capture_device *dev) {
    if (dev == NULL || dev->session == nil)
        return 0;
    [dev->session startRunning];
    // Block for the first sample buffer: the render loop breaks on any
    // failed read(), and AVFoundation typically delivers frame 0 several
    // hundred ms after startRunning.
    if (dev->delegate != nil) {
        std::unique_lock<std::mutex> lock(dev->delegate->frame_mutex);
        if (!dev->delegate->has_frame) {
            dev->delegate->frame_ready.wait_for(lock, std::chrono::milliseconds(2000));
        }
    }
    return 1;
}

static int avf_read(capture_device *dev, capture_frame *frame) {
    if (dev == NULL || frame == NULL || dev->delegate == nil)
        return 0;

    {
        std::unique_lock<std::mutex> lock(dev->delegate->frame_mutex);
        if (!dev->delegate->has_frame) {
            dev->delegate->frame_ready.wait_for(lock, std::chrono::milliseconds(250));
        }
        if (!dev->delegate->has_frame || dev->delegate->frame_data.empty())
            return 0;
        frame->data = dev->delegate->frame_data.data();
        frame->data_size = dev->delegate->frame_data.size();
        frame->width = dev->delegate->frame_width;
        frame->height = dev->delegate->frame_height;
        frame->stride_bytes = dev->delegate->frame_stride;
        frame->pixel_format = CAPTURE_PIXFMT_BGRA32;
    }
    return 1;
}

static void avf_release(capture_device *dev, capture_frame *frame) {
    (void)dev;
    if (frame != NULL)
        memset(frame, 0, sizeof(*frame));
}

static void avf_stop(capture_device *dev) {
    if (dev == NULL || dev->session == nil)
        return;
    [dev->session stopRunning];
}

static void avf_close(capture_device *dev) {
    if (dev == NULL)
        return;
    avf_stop(dev);
    if (dev->output != nil)
        [dev->output setSampleBufferDelegate:nil queue:NULL];
    free(dev);
}

static int avf_list_controls(capture_device *dev, capture_control_desc *out, int max_controls) {
    (void)dev;
    (void)out;
    (void)max_controls;
    return 0;
}

static int avf_set_control(capture_device *dev, capture_control_id id, int value) {
    (void)dev;
    (void)id;
    (void)value;
    return 0;
}

static int avf_set_control_auto(capture_device *dev, capture_control_id id, int enabled) {
    (void)dev;
    (void)id;
    (void)enabled;
    return 0;
}

static const char *avf_name(void) {
    return "avfoundation";
}

static const capture_ops ops = {
    avf_open,
    avf_describe,
    avf_start,
    avf_read,
    avf_release,
    avf_stop,
    avf_close,
    avf_name,
    avf_list_controls,
    avf_set_control,
    avf_set_control_auto
};

const capture_ops *capture_avfoundation_ops(void) {
    return &ops;
}

#else

const capture_ops *capture_avfoundation_ops(void) {
    return NULL;
}

#endif
