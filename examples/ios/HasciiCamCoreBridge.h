#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface HasciiCamCoreBridge : NSObject

- (instancetype)init;
- (void)dealloc;

- (BOOL)startWithCameraWidth:(int)cameraWidth
                cameraHeight:(int)cameraHeight
                  asciiWidth:(int)asciiWidth
                 asciiHeight:(int)asciiHeight;

- (BOOL)submitFrame:(const unsigned char *)data
               size:(NSUInteger)size
              width:(int)width
             height:(int)height
             stride:(int)stride;

- (nullable NSString *)renderASCII;
- (void)stop;

@end

NS_ASSUME_NONNULL_END
