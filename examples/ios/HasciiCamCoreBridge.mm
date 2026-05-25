#import "HasciiCamCoreBridge.h"

#include <hasciicam/hasciicam.h>

@implementation HasciiCamCoreBridge {
    hasciicam_instance *_instance;
}

- (instancetype)init {
    self = [super init];
    if (!self) {
        return nil;
    }
    _instance = hasciicam_create();
    return self;
}

- (void)dealloc {
    if (_instance != NULL) {
        hasciicam_destroy(_instance);
        _instance = NULL;
    }
}

- (BOOL)startWithCameraWidth:(int)cameraWidth
                cameraHeight:(int)cameraHeight
                  asciiWidth:(int)asciiWidth
                 asciiHeight:(int)asciiHeight {
    if (_instance == NULL) {
        return NO;
    }
    return hasciicam_start_external(_instance, cameraWidth, cameraHeight, asciiWidth, asciiHeight) ? YES : NO;
}

- (BOOL)submitFrame:(const unsigned char *)data
               size:(NSUInteger)size
              width:(int)width
             height:(int)height
             stride:(int)stride {
    if (_instance == NULL || data == NULL || size == 0) {
        return NO;
    }
    return hasciicam_submit_frame(_instance, data, size, width, height, stride, HASCIICAM_PIXFMT_BGRA32) ? YES : NO;
}

- (nullable NSString *)renderASCII {
    const char *text = NULL;
    int width = 0;
    int height = 0;
    size_t length;
    if (_instance == NULL) {
        return nil;
    }
    if (!hasciicam_render_frame(_instance)) {
        return nil;
    }
    if (!hasciicam_get_ascii_frame(_instance, &text, NULL, &width, &height) || text == NULL || width <= 0 || height <= 0) {
        return nil;
    }
    length = (size_t)width * (size_t)height;
    return [[NSString alloc] initWithBytes:text length:length encoding:NSASCIIStringEncoding];
}

- (void)stop {
    if (_instance != NULL) {
        hasciicam_stop(_instance);
    }
}

@end
