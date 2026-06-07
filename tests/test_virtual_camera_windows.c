#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "../src/virtual_camera/windows/source/hasciicam_virtual_camera_source.h"
#include "../src/virtual_camera/windows/pipe/hasciicam_virtual_camera_pipe.h"

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

    {
        hasciicam_virtual_camera_request request;
        hasciicam_virtual_camera_source_config config;
        char pipe_name[256];
        char payload[256];
        char err[128];

        hasciicam_virtual_camera_request_init(&request);
        request.enabled = 1;
        strncpy(request.device, "Device:One/Two", sizeof(request.device) - 1);
        request.device[sizeof(request.device) - 1] = '\0';

        expect_true(hasciicam_virtual_camera_pipe_build_name(&request, pipe_name, sizeof(pipe_name), err, sizeof(err)),
                    "pipe name should be generated for an enabled request");
        if (hasciicam_virtual_camera_pipe_build_name(&request, pipe_name, sizeof(pipe_name), err, sizeof(err))) {
            expect_true(strstr(pipe_name, "\\\\.\\pipe\\HasciiCam\\") == pipe_name,
                        "pipe name should use the expected prefix");
            expect_true(strstr(pipe_name, "\\Device_One_Two\\") != NULL,
                        "pipe name should sanitize device characters");
            expect_true(strstr(pipe_name, "\\1280x720@30") != NULL,
                        "pipe name should encode geometry and fps");
        }

        expect_true(hasciicam_virtual_camera_pipe_build_registration_payload(&request,
                                                                             payload,
                                                                             sizeof(payload),
                                                                             err,
                                                                             sizeof(err)),
                    "registration payload should be generated");
        if (hasciicam_virtual_camera_pipe_build_registration_payload(&request,
                                                                     payload,
                                                                     sizeof(payload),
                                                                     err,
                                                                     sizeof(err))) {
            expect_true(strstr(payload, "v=1;") == payload,
                        "payload should start with the version");
            expect_true(strstr(payload, "pipe=\\\\.\\pipe\\HasciiCam\\") != NULL,
                        "payload should include the pipe name");
            expect_true(strstr(payload, "device=Device_One_Two;") != NULL,
                        "payload should sanitize the device name");
            expect_true(strstr(payload, "size=1280x720;") != NULL,
                        "payload should include geometry");
            expect_true(strstr(payload, "fps=30;") != NULL,
                        "payload should include fps");
            expect_true(strstr(payload, "fmt=yuy2;") != NULL,
                        "payload should include the frame format");
        }

        expect_true(hasciicam_virtual_camera_source_pipe_name(&request, pipe_name, sizeof(pipe_name), err, sizeof(err)),
                    "source pipe name wrapper should succeed");
        expect_true(hasciicam_virtual_camera_source_registration_payload(&request,
                                                                         payload,
                                                                         sizeof(payload),
                                                                         err,
                                                                         sizeof(err)),
                    "source payload wrapper should succeed");
        expect_true(hasciicam_virtual_camera_source_pipe_sddl(payload,
                                                              sizeof(payload),
                                                              err,
                                                              sizeof(err)),
                    "source SDDL wrapper should succeed");
        if (hasciicam_virtual_camera_source_pipe_sddl(payload,
                                                      sizeof(payload),
                                                      err,
                                                      sizeof(err))) {
            expect_true(strstr(payload, "D:P(") == payload,
                        "security descriptor should start with a protected DACL");
            expect_true(strstr(payload, "SY") != NULL,
                        "security descriptor should allow SYSTEM");
            expect_true(strstr(payload, "LS") != NULL,
                        "security descriptor should allow Local Service");
            expect_true(strstr(payload, "S-1-5-") != NULL,
                        "security descriptor should include the current user SID");
        }

        expect_true(hasciicam_virtual_camera_source_config_prepare(&request, &config, err, sizeof(err)),
                    "source config should prepare successfully");
        if (hasciicam_virtual_camera_source_config_prepare(&request, &config, err, sizeof(err))) {
            expect_true(config.media_type_count == 2, "prepared source config should include two media types");
            expect_true(strcmp(config.pipe_name, pipe_name) == 0, "prepared source config should reuse the pipe name");
            expect_true(strstr(config.registration_payload, "fmt=yuy2;") != NULL,
                        "prepared source config should include the registration payload");
            expect_true(strstr(config.pipe_sddl, "SY") != NULL, "prepared source config should include the pipe SDDL");
        }
    }

    {
        hasciicam_virtual_camera_pipe_frame frame;
        char err[128];
        unsigned char *message = NULL;
        hasciicam_virtual_camera_pipe_frame decoded;
        const unsigned char *payload = NULL;
        size_t payload_size = 0;
        size_t message_size = sizeof(frame) + 1843200U;

        hasciicam_virtual_camera_pipe_frame_init(&frame,
                                                 HASCIICAM_VIRTUAL_CAMERA_PIXFMT_YUY2,
                                                 1280,
                                                 720,
                                                 2560,
                                                 7ULL,
                                                 123456789ULL);
        message = (unsigned char *)malloc(message_size);
        expect_true(message != NULL, "test message should allocate");
        if (message == NULL)
            goto cleanup_message;
        memcpy(message, &frame, sizeof(frame));
        memset(message + sizeof(frame), 0x80, 1843200U);
        expect_true(frame.magic == HASCIICAM_VIRTUAL_CAMERA_PIPE_MAGIC,
                    "pipe frame should be initialized with the project magic");
        expect_true(frame.version == HASCIICAM_VIRTUAL_CAMERA_PIPE_VERSION,
                    "pipe frame should be initialized with version 1");
        expect_true(frame.header_size == sizeof(frame),
                    "pipe frame should advertise the packed header size");
        expect_true(frame.payload_bytes == 1843200U,
                    "pipe frame payload should match YUY2 geometry");
        expect_true(hasciicam_virtual_camera_pipe_frame_validate(&frame, err, sizeof(err)),
                    "valid YUY2 pipe frame should pass validation");
        if (!hasciicam_virtual_camera_pipe_frame_validate(&frame, err, sizeof(err)))
            fprintf(stderr, "unexpected validation error: %s\n", err);

        frame.payload_bytes -= 1U;
        expect_true(!hasciicam_virtual_camera_pipe_frame_validate(&frame, err, sizeof(err)),
                    "truncated pipe frame should be rejected");
        frame.payload_bytes = 1843200U;

        expect_true(hasciicam_virtual_camera_pipe_decode_message(message,
                                                                 message_size,
                                                                 &decoded,
                                                                 &payload,
                                                                 &payload_size,
                                                                 err,
                                                                 sizeof(err)),
                    "exact pipe message should decode");
        if (hasciicam_virtual_camera_pipe_decode_message(message,
                                                         sizeof(message),
                                                         &decoded,
                                                         &payload,
                                                         &payload_size,
                                                         err,
                                                         sizeof(err))) {
            expect_true(decoded.payload_bytes == 1843200U, "decoded header should preserve payload size");
            expect_true(payload_size == 1843200U, "decoded payload size should match");
            expect_true(payload == message + sizeof(frame), "decoded payload pointer should reference the message body");
        }

        expect_true(!hasciicam_virtual_camera_pipe_decode_message(message,
                                                                  message_size - 1U,
                                                                  &decoded,
                                                                  &payload,
                                                                  &payload_size,
                                                                  err,
                                                                  sizeof(err)),
                    "truncated message should be rejected");

        message[4] ^= 0x01;
        expect_true(!hasciicam_virtual_camera_pipe_decode_message(message,
                                                                  message_size,
                                                                  &decoded,
                                                                  &payload,
                                                                  &payload_size,
                                                                  err,
                                                                  sizeof(err)),
                    "malformed message version should be rejected");

        message[4] ^= 0x01;
        expect_true(hasciicam_virtual_camera_pipe_encode_message(&frame,
                                                                 message + sizeof(frame),
                                                                 1843200U,
                                                                 message,
                                                                 message_size,
                                                                 err,
                                                                 sizeof(err)),
                    "exact pipe message should encode");
        expect_true(hasciicam_virtual_camera_pipe_decode_message(message,
                                                                 message_size,
                                                                 &decoded,
                                                                 &payload,
                                                                 &payload_size,
                                                                 err,
                                                                 sizeof(err)),
                    "encoded pipe message should decode");

        {
            hasciicam_virtual_camera_pipe_frame oversized = frame;
            oversized.width = HASCIICAM_VIRTUAL_CAMERA_PIPE_MAX_WIDTH + 2;
            oversized.payload_bytes = 0;
            expect_true(!hasciicam_virtual_camera_pipe_frame_validate(&oversized, err, sizeof(err)),
                        "oversized pipe frame should be rejected");
        }

cleanup_message:
        free(message);
    }

    {
        hasciicam_virtual_camera_source_frame_slot slot;
        hasciicam_virtual_camera_pipe_frame frame;
        unsigned char *message = NULL;
        char err[128];
        size_t message_size = sizeof(frame) + 1843200U;
        unsigned long long first_sequence;

        hasciicam_virtual_camera_source_frame_slot_init(&slot);
        hasciicam_virtual_camera_pipe_frame_init(&frame,
                                                 HASCIICAM_VIRTUAL_CAMERA_PIXFMT_YUY2,
                                                 1280,
                                                 720,
                                                 2560,
                                                 11ULL,
                                                 222222222ULL);
        message = (unsigned char *)malloc(message_size);
        expect_true(message != NULL, "slot test message should allocate");
        if (message != NULL) {
            memcpy(message, &frame, sizeof(frame));
            memset(message + sizeof(frame), 0x7f, 1843200U);
            expect_true(hasciicam_virtual_camera_source_frame_slot_store(&slot,
                                                                         message,
                                                                         message_size,
                                                                         err,
                                                                         sizeof(err)),
                        "slot should store the newest complete message");
            expect_true(hasciicam_virtual_camera_source_frame_slot_has_message(&slot),
                        "slot should report a buffered message");
            first_sequence = slot.sequence;
            frame.sequence = 12ULL;
            frame.timestamp_100ns = 333333333ULL;
            memcpy(message, &frame, sizeof(frame));
            expect_true(hasciicam_virtual_camera_source_frame_slot_store(&slot,
                                                                         message,
                                                                         message_size,
                                                                         err,
                                                                         sizeof(err)),
                        "slot should replace the older message");
            expect_true(slot.sequence == 12ULL && slot.timestamp_100ns == 333333333ULL,
                        "slot should retain the newest message metadata");
            expect_true(slot.sequence != first_sequence, "slot should replace the previous sequence");
            expect_true(!hasciicam_virtual_camera_source_frame_slot_store(&slot,
                                                                         message,
                                                                         message_size - 1U,
                                                                         err,
                                                                         sizeof(err)),
                        "slot should reject truncated messages");
            hasciicam_virtual_camera_source_frame_slot_close(&slot);
            expect_true(!hasciicam_virtual_camera_source_frame_slot_has_message(&slot),
                        "closed slot should be empty");
            free(message);
        }
    }

    {
        hasciicam_virtual_camera_request request;
        hasciicam_virtual_camera_source_config config;
        unsigned char *message = NULL;
        size_t message_size;
        char err[128];

        hasciicam_virtual_camera_request_init(&request);
        request.enabled = 1;
        expect_true(hasciicam_virtual_camera_source_config_prepare(&request, &config, err, sizeof(err)),
                    "black frame test should prepare config");
        message_size = sizeof(hasciicam_virtual_camera_pipe_frame) + 1843200U;
        message = (unsigned char *)malloc(message_size);
        expect_true(message != NULL, "black message should allocate");
        if (message != NULL) {
            expect_true(hasciicam_virtual_camera_source_make_black_message(&config,
                                                                           99ULL,
                                                                           444444444ULL,
                                                                           message,
                                                                           message_size,
                                                                           err,
                                                                           sizeof(err)),
                        "black frame should be generated");
            if (hasciicam_virtual_camera_source_make_black_message(&config,
                                                                   99ULL,
                                                                   444444444ULL,
                                                                   message,
                                                                   message_size,
                                                                   err,
                                                                   sizeof(err))) {
                expect_true(message[sizeof(hasciicam_virtual_camera_pipe_frame) + 0] == 0x10,
                            "black frame should start with luma 16");
                expect_true(message[sizeof(hasciicam_virtual_camera_pipe_frame) + 1] == 0x80,
                            "black frame should use neutral chroma");
                expect_true(message[sizeof(hasciicam_virtual_camera_pipe_frame) + 2] == 0x10,
                            "black frame should repeat luma 16");
                expect_true(message[sizeof(hasciicam_virtual_camera_pipe_frame) + 3] == 0x80,
                            "black frame should repeat neutral chroma");
            }
            free(message);
        }
    }

    {
        unsigned long long duration = hasciicam_virtual_camera_source_sample_duration_100ns(30);
        unsigned long long start = 987654321ULL;
        expect_true(duration == 333333ULL, "sample duration should match 30 fps");
        expect_true(hasciicam_virtual_camera_source_sample_time_100ns(start, 0ULL, 30) == start,
                    "first sample should start at the provided timestamp");
        expect_true(hasciicam_virtual_camera_source_sample_time_100ns(start, 1ULL, 30) == start + duration,
                    "second sample should advance by one duration");
        expect_true(hasciicam_virtual_camera_source_sample_time_100ns(start, 9ULL, 30) == start + duration * 9ULL,
                    "sample timestamps should advance monotonically");
    }

    {
        hasciicam_virtual_camera_request request;
        hasciicam_virtual_camera_source_config config;
        hasciicam_virtual_camera_source_lifecycle lifecycle;
        char err[128];

        hasciicam_virtual_camera_request_init(&request);
        request.enabled = 1;
        expect_true(hasciicam_virtual_camera_source_config_prepare(&request, &config, err, sizeof(err)),
                    "config should prepare for lifecycle test");
        hasciicam_virtual_camera_source_lifecycle_init(&lifecycle, &config);
        expect_true(lifecycle.state == HASCIICAM_VIRTUAL_CAMERA_SOURCE_STATE_CREATED,
                    "lifecycle should start in created state");
        expect_true(hasciicam_virtual_camera_source_lifecycle_start(&lifecycle, err, sizeof(err)),
                    "lifecycle should start");
        expect_true(lifecycle.state == HASCIICAM_VIRTUAL_CAMERA_SOURCE_STATE_STARTED,
                    "lifecycle should move to started");
        expect_true(hasciicam_virtual_camera_source_lifecycle_stop(&lifecycle, err, sizeof(err)),
                    "lifecycle should stop");
        expect_true(lifecycle.state == HASCIICAM_VIRTUAL_CAMERA_SOURCE_STATE_STOPPED,
                    "lifecycle should move to stopped");
        expect_true(hasciicam_virtual_camera_source_lifecycle_shutdown(&lifecycle, err, sizeof(err)),
                    "lifecycle should shutdown");
        expect_true(lifecycle.state == HASCIICAM_VIRTUAL_CAMERA_SOURCE_STATE_SHUTDOWN,
                    "lifecycle should move to shutdown");
        expect_true(!hasciicam_virtual_camera_source_lifecycle_start(&lifecycle, err, sizeof(err)),
                    "shutdown lifecycle should not restart");
    }

    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
