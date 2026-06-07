#include <stdio.h>
#include <wchar.h>

#include "../src/virtual_camera/windows/source/hasciicam_virtual_camera_source.h"

static int failures = 0;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        failures++;
    }
}

int main(void) {
    const wchar_t *clsid = hasciicam_virtual_camera_source_clsid_string();
    expect_true(clsid != NULL, "clsid string should exist");
    expect_true(wcscmp(clsid, L"{29E1D0B1-0AF8-4D6F-9D5E-0F9A0F0D4F58}") == 0,
                "clsid string should match the declared source id");
    expect_true(hasciicam_virtual_camera_source_clsid() != NULL,
                "clsid pointer should exist");
    expect_true(hasciicam_virtual_camera_source_media_type_count() == 2,
                "source should advertise YUY2 and NV12 media types");

    {
        hasciicam_virtual_camera_source_media_type media_type;
        int ok = hasciicam_virtual_camera_source_media_type_get(0, 1280, 720, 30, &media_type);
        expect_true(ok, "YUY2 media type should describe configured geometry");
        if (ok) {
            expect_true(media_type.pixel_format == HASCIICAM_VIRTUAL_CAMERA_PIXFMT_YUY2,
                        "first media type should be YUY2");
            expect_true(media_type.subtype != NULL, "YUY2 subtype GUID should exist");
            expect_true(wcscmp(media_type.subtype_name, L"MFVideoFormat_YUY2") == 0,
                        "YUY2 subtype name should be stable");
            expect_true(media_type.width == 1280 && media_type.height == 720,
                        "YUY2 media type should keep configured dimensions");
            expect_true(media_type.fps == 30, "YUY2 media type should keep configured fps");
            expect_true(media_type.stride_bytes == 2560, "YUY2 stride should be packed");
            expect_true(media_type.frame_bytes == 1843200ULL, "YUY2 frame size should match packed geometry");
            expect_true(media_type.sample_duration_100ns == 333333ULL,
                        "YUY2 sample duration should match 30 fps");
            expect_true(media_type.average_bitrate == 442368000ULL,
                        "YUY2 bitrate should match geometry and frame rate");
            expect_true(media_type.progressive == 1, "YUY2 should be progressive");
            expect_true(media_type.square_pixels == 1, "YUY2 should use square pixels");
            expect_true(media_type.stream_id == 0, "YUY2 stream id should be 0");
            expect_true(media_type.frameserver_shared == 1, "YUY2 stream should be shared");
            expect_true(media_type.framesource_color == 1, "YUY2 stream should be color");
        }
    }

    {
        hasciicam_virtual_camera_source_media_type media_type;
        int ok = hasciicam_virtual_camera_source_media_type_get(1, 1280, 720, 30, &media_type);
        expect_true(ok, "NV12 media type should describe configured geometry");
        if (ok) {
            expect_true(media_type.pixel_format == HASCIICAM_VIRTUAL_CAMERA_PIXFMT_NV12,
                        "second media type should be NV12");
            expect_true(media_type.subtype != NULL, "NV12 subtype GUID should exist");
            expect_true(wcscmp(media_type.subtype_name, L"MFVideoFormat_NV12") == 0,
                        "NV12 subtype name should be stable");
            expect_true(media_type.stride_bytes == 1280, "NV12 stride should follow luma width");
            expect_true(media_type.frame_bytes == 1382400ULL, "NV12 frame size should match packed geometry");
            expect_true(media_type.sample_duration_100ns == 333333ULL,
                        "NV12 sample duration should match 30 fps");
            expect_true(media_type.average_bitrate == 331776000ULL,
                        "NV12 bitrate should match geometry and frame rate");
        }
    }

    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
