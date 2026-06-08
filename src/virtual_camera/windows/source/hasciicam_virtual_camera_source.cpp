#include "hasciicam_virtual_camera_source.h"

#if defined(_WIN32)

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mfobjects.h>
#include <ks.h>
#include <ksproxy.h>
#include <ksmedia.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unknwn.h>
#include <stdarg.h>
#include <new>

#include "../pipe/hasciicam_virtual_camera_pipe.h"

static const GUID kHasciiCamVirtualCameraSourceClsid =
{ 0x29e1d0b1, 0x0af8, 0x4d6f, { 0x9d, 0x5e, 0x0f, 0x9a, 0x0f, 0x0d, 0x4f, 0x58 } };

static const wchar_t kHasciiCamVirtualCameraSourceClsidString[] =
    L"{29E1D0B1-0AF8-4D6F-9D5E-0F9A0F0D4F58}";

static LONG g_module_refcount = 0;
static HINSTANCE g_module_instance = NULL;

static void hasciicam_virtual_camera_source_trace(const char *format, ...) {
    char line[512];
    char path[MAX_PATH];
    char *slash;
    DWORD length;
    DWORD written;
    HANDLE file;
    va_list args;
    int prefix_length;
    int message_length;

    if (g_module_instance == NULL || format == NULL)
        return;
    length = GetModuleFileNameA(g_module_instance, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return;
    slash = strrchr(path, '\\');
    if (slash == NULL)
        return;
    strcpy(slash + 1, "hasciicam_virtual_camera_source.log");
    file = CreateFileA(path,
                       FILE_APPEND_DATA,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,
                       NULL);
    if (file == INVALID_HANDLE_VALUE)
        return;
    prefix_length = snprintf(line,
                             sizeof(line),
                             "%llu pid=%lu tid=%lu ",
                             (unsigned long long)GetTickCount64(),
                             (unsigned long)GetCurrentProcessId(),
                             (unsigned long)GetCurrentThreadId());
    if (prefix_length < 0 || (size_t)prefix_length >= sizeof(line)) {
        CloseHandle(file);
        return;
    }
    va_start(args, format);
    message_length = vsnprintf(line + prefix_length,
                               sizeof(line) - (size_t)prefix_length,
                               format,
                               args);
    va_end(args);
    if (message_length < 0)
        message_length = 0;
    length = (DWORD)prefix_length + (DWORD)message_length;
    if (length > sizeof(line) - 3)
        length = sizeof(line) - 3;
    line[length++] = '\r';
    line[length++] = '\n';
    WriteFile(file, line, length, &written, NULL);
    CloseHandle(file);
}

static const hasciicam_virtual_camera_source_media_type kSupportedMediaTypes[] = {
    {
        HASCIICAM_VIRTUAL_CAMERA_PIXFMT_YUY2,
        L"MFVideoFormat_YUY2",
        &MFVideoFormat_YUY2,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        1,
        0,
        1,
        1
    }
};

static int hasciicam_virtual_camera_media_type_validate(int width,
                                                        int height,
                                                        int fps,
                                                        char *err,
                                                        size_t err_size) {
    if (width <= 0 || height <= 0) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "virtual camera size must be positive");
        return 0;
    }
    if ((width & 1) != 0 || (height & 1) != 0) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "virtual camera size must be even");
        return 0;
    }
    if (fps <= 0) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "virtual camera fps must be positive");
        return 0;
    }
    return 1;
}

static unsigned long long hasciicam_virtual_camera_media_type_duration_100ns(int fps) {
    return fps > 0 ? 10000000ULL / (unsigned long long)fps : 0ULL;
}

static unsigned long long hasciicam_virtual_camera_media_type_bitrate(int width,
                                                                      int height,
                                                                      int fps,
                                                                      hasciicam_virtual_camera_pixel_format pixel_format) {
    unsigned long long bits_per_pixel = 0ULL;
    switch (pixel_format) {
    case HASCIICAM_VIRTUAL_CAMERA_PIXFMT_YUY2:
        bits_per_pixel = 16ULL;
        break;
    case HASCIICAM_VIRTUAL_CAMERA_PIXFMT_NV12:
        bits_per_pixel = 12ULL;
        break;
    default:
        bits_per_pixel = 0ULL;
        break;
    }
    return (unsigned long long)width * (unsigned long long)height * bits_per_pixel * (unsigned long long)fps;
}

const GUID *hasciicam_virtual_camera_source_clsid(void) {
    return &kHasciiCamVirtualCameraSourceClsid;
}

const wchar_t *hasciicam_virtual_camera_source_clsid_string(void) {
    return kHasciiCamVirtualCameraSourceClsidString;
}

size_t hasciicam_virtual_camera_source_media_type_count(void) {
    return sizeof(kSupportedMediaTypes) / sizeof(kSupportedMediaTypes[0]);
}

int hasciicam_virtual_camera_source_media_type_get(size_t index,
                                                   int width,
                                                   int height,
                                                   int fps,
                                                   hasciicam_virtual_camera_source_media_type *out) {
    char errbuf[96];
    if (out == NULL)
        return 0;
    if (index >= hasciicam_virtual_camera_source_media_type_count())
        return 0;
    if (!hasciicam_virtual_camera_media_type_validate(width, height, fps, errbuf, sizeof(errbuf)))
        return 0;

    *out = kSupportedMediaTypes[index];
    out->width = width;
    out->height = height;
    out->fps = fps;
    out->stride_bytes = (out->pixel_format == HASCIICAM_VIRTUAL_CAMERA_PIXFMT_NV12)
                            ? width
                            : width * 2;
    if (out->pixel_format == HASCIICAM_VIRTUAL_CAMERA_PIXFMT_YUY2) {
        out->frame_bytes = hasciicam_virtual_camera_yuy2_size(width, height, out->stride_bytes);
    } else if (out->pixel_format == HASCIICAM_VIRTUAL_CAMERA_PIXFMT_NV12) {
        out->frame_bytes = hasciicam_virtual_camera_nv12_size(width, height, width, width);
    } else {
        out->frame_bytes = 0;
    }
    out->sample_duration_100ns = hasciicam_virtual_camera_media_type_duration_100ns(fps);
    out->average_bitrate = hasciicam_virtual_camera_media_type_bitrate(width, height, fps, out->pixel_format);
    return 1;
}

int hasciicam_virtual_camera_source_pipe_name(const hasciicam_virtual_camera_request *request,
                                              char *out,
                                              size_t out_size,
                                              char *err,
                                              size_t err_size) {
    return hasciicam_virtual_camera_pipe_build_name(request, out, out_size, err, err_size);
}

int hasciicam_virtual_camera_source_registration_payload(const hasciicam_virtual_camera_request *request,
                                                         char *out,
                                                         size_t out_size,
                                                         char *err,
                                                         size_t err_size) {
    return hasciicam_virtual_camera_pipe_build_registration_payload(request, out, out_size, err, err_size);
}

int hasciicam_virtual_camera_source_pipe_sddl(char *out,
                                              size_t out_size,
                                              char *err,
                                              size_t err_size) {
    return hasciicam_virtual_camera_pipe_build_sddl(out, out_size, err, err_size);
}

int hasciicam_virtual_camera_source_config_prepare(const hasciicam_virtual_camera_request *request,
                                                   hasciicam_virtual_camera_source_config *out,
                                                   char *err,
                                                   size_t err_size) {
    size_t i;

    if (request == NULL || out == NULL) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "source request and output config are required");
        return 0;
    }
    memset(out, 0, sizeof(*out));
    out->request = *request;
    if (!hasciicam_virtual_camera_source_pipe_name(request, out->pipe_name, sizeof(out->pipe_name), err, err_size))
        return 0;
    if (!hasciicam_virtual_camera_source_registration_payload(request,
                                                              out->registration_payload,
                                                              sizeof(out->registration_payload),
                                                              err,
                                                              err_size))
        return 0;
    if (!hasciicam_virtual_camera_source_pipe_sddl(out->pipe_sddl, sizeof(out->pipe_sddl), err, err_size))
        return 0;
    out->media_type_count = hasciicam_virtual_camera_source_media_type_count();
    if (out->media_type_count > sizeof(out->media_types) / sizeof(out->media_types[0]))
        out->media_type_count = sizeof(out->media_types) / sizeof(out->media_types[0]);
    for (i = 0; i < out->media_type_count; ++i) {
        if (!hasciicam_virtual_camera_source_media_type_get(i,
                                                            request->width,
                                                            request->height,
                                                            request->fps,
                                                            &out->media_types[i])) {
            if (err != NULL && err_size > 0)
                snprintf(err, err_size, "failed to prepare media type %zu", i);
            return 0;
        }
    }
    return 1;
}

static int hasciicam_virtual_camera_source_lifecycle_state_transition(hasciicam_virtual_camera_source_lifecycle *lifecycle,
                                                                       hasciicam_virtual_camera_source_state next_state,
                                                                       char *err,
                                                                       size_t err_size) {
    if (lifecycle == NULL) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "source lifecycle is required");
        return 0;
    }
    if (lifecycle->state == HASCIICAM_VIRTUAL_CAMERA_SOURCE_STATE_SHUTDOWN) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "source lifecycle is already shut down");
        return 0;
    }
    if (lifecycle->state == next_state)
        return 1;
    if (next_state == HASCIICAM_VIRTUAL_CAMERA_SOURCE_STATE_STARTED) {
        if (lifecycle->state != HASCIICAM_VIRTUAL_CAMERA_SOURCE_STATE_CREATED &&
            lifecycle->state != HASCIICAM_VIRTUAL_CAMERA_SOURCE_STATE_STOPPED) {
            if (err != NULL && err_size > 0)
                snprintf(err, err_size, "source can only start from created or stopped");
            return 0;
        }
        lifecycle->state = next_state;
        return 1;
    }
    if (next_state == HASCIICAM_VIRTUAL_CAMERA_SOURCE_STATE_STOPPED) {
        if (lifecycle->state != HASCIICAM_VIRTUAL_CAMERA_SOURCE_STATE_STARTED) {
            if (err != NULL && err_size > 0)
                snprintf(err, err_size, "source can only stop from started");
            return 0;
        }
        lifecycle->state = next_state;
        return 1;
    }
    if (next_state == HASCIICAM_VIRTUAL_CAMERA_SOURCE_STATE_SHUTDOWN) {
        lifecycle->state = next_state;
        return 1;
    }
    if (err != NULL && err_size > 0)
        snprintf(err, err_size, "invalid source lifecycle transition");
    return 0;
}

void hasciicam_virtual_camera_source_lifecycle_init(hasciicam_virtual_camera_source_lifecycle *lifecycle,
                                                    const hasciicam_virtual_camera_source_config *config) {
    if (lifecycle == NULL)
        return;
    memset(lifecycle, 0, sizeof(*lifecycle));
    lifecycle->state = HASCIICAM_VIRTUAL_CAMERA_SOURCE_STATE_CREATED;
    if (config != NULL)
        lifecycle->config = *config;
}

int hasciicam_virtual_camera_source_lifecycle_start(hasciicam_virtual_camera_source_lifecycle *lifecycle,
                                                    char *err,
                                                    size_t err_size) {
    return hasciicam_virtual_camera_source_lifecycle_state_transition(lifecycle,
                                                                      HASCIICAM_VIRTUAL_CAMERA_SOURCE_STATE_STARTED,
                                                                      err,
                                                                      err_size);
}

int hasciicam_virtual_camera_source_lifecycle_stop(hasciicam_virtual_camera_source_lifecycle *lifecycle,
                                                   char *err,
                                                   size_t err_size) {
    return hasciicam_virtual_camera_source_lifecycle_state_transition(lifecycle,
                                                                      HASCIICAM_VIRTUAL_CAMERA_SOURCE_STATE_STOPPED,
                                                                      err,
                                                                      err_size);
}

int hasciicam_virtual_camera_source_lifecycle_shutdown(hasciicam_virtual_camera_source_lifecycle *lifecycle,
                                                       char *err,
                                                       size_t err_size) {
    return hasciicam_virtual_camera_source_lifecycle_state_transition(lifecycle,
                                                                      HASCIICAM_VIRTUAL_CAMERA_SOURCE_STATE_SHUTDOWN,
                                                                      err,
                                                                      err_size);
}

void hasciicam_virtual_camera_source_frame_slot_init(hasciicam_virtual_camera_source_frame_slot *slot) {
    if (slot == NULL)
        return;
    memset(slot, 0, sizeof(*slot));
}

void hasciicam_virtual_camera_source_frame_slot_close(hasciicam_virtual_camera_source_frame_slot *slot) {
    if (slot == NULL)
        return;
    free(slot->bytes);
    slot->bytes = NULL;
    slot->bytes_size = 0;
    slot->capacity = 0;
    slot->sequence = 0;
    slot->timestamp_100ns = 0;
}

int hasciicam_virtual_camera_source_frame_slot_has_message(const hasciicam_virtual_camera_source_frame_slot *slot) {
    return slot != NULL && slot->bytes != NULL && slot->bytes_size > 0;
}

int hasciicam_virtual_camera_source_read_pipe_message(const hasciicam_virtual_camera_source_config *config,
                                                      const char *pipe_name,
                                                      hasciicam_virtual_camera_source_frame_slot *slot,
                                                      int timeout_ms,
                                                      char *err,
                                                      size_t err_size) {
    HANDLE pipe_handle = INVALID_HANDLE_VALUE;
    unsigned char *message = NULL;
    size_t message_size;
    DWORD bytes_read = 0;
    DWORD total_read = 0;
    hasciicam_virtual_camera_pipe_frame header;
    const unsigned char *payload = NULL;
    size_t payload_size = 0;
    ULONGLONG deadline;
    int ok = 0;

    if (config == NULL || pipe_name == NULL || slot == NULL) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "source config, pipe name, and slot are required");
        return 0;
    }
    if (config->media_type_count == 0) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "source config does not describe a media type");
        return 0;
    }
    message_size = sizeof(hasciicam_virtual_camera_pipe_frame) + config->media_types[0].frame_bytes;
    message = (unsigned char *)malloc(message_size);
    if (message == NULL) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "unable to allocate source pipe buffer");
        return 0;
    }

    if (timeout_ms < 0)
        timeout_ms = 0;
    deadline = GetTickCount64() + (ULONGLONG)timeout_ms;
    while (pipe_handle == INVALID_HANDLE_VALUE) {
        pipe_handle = CreateFileA(pipe_name,
                                  GENERIC_READ,
                                  0,
                                  NULL,
                                  OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL,
                                  NULL);
        if (pipe_handle != INVALID_HANDLE_VALUE)
            break;
        if (GetLastError() != ERROR_PIPE_BUSY && GetLastError() != ERROR_FILE_NOT_FOUND)
            break;
        if (GetTickCount64() >= deadline)
            break;
        Sleep(10);
    }
    if (pipe_handle == INVALID_HANDLE_VALUE) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "unable to open virtual camera pipe");
        goto cleanup;
    }

    while (total_read < (DWORD)message_size) {
        if (!ReadFile(pipe_handle,
                      message + total_read,
                      (DWORD)message_size - total_read,
                      &bytes_read,
                      NULL) ||
            bytes_read == 0) {
            if (err != NULL && err_size > 0)
                snprintf(err, err_size, "unable to read virtual camera pipe message");
            goto cleanup;
        }
        total_read += bytes_read;
    }
    if (!hasciicam_virtual_camera_pipe_decode_message(message,
                                                      total_read,
                                                      &header,
                                                      &payload,
                                                      &payload_size,
                                                      err,
                                                      err_size)) {
        goto cleanup;
    }
    if (!hasciicam_virtual_camera_source_frame_slot_store(slot,
                                                           message,
                                                           sizeof(hasciicam_virtual_camera_pipe_frame) + payload_size,
                                                           err,
                                                           err_size)) {
        goto cleanup;
    }
    ok = 1;

cleanup:
    if (pipe_handle != INVALID_HANDLE_VALUE)
        CloseHandle(pipe_handle);
    free(message);
    return ok;
}

int hasciicam_virtual_camera_source_frame_slot_store(hasciicam_virtual_camera_source_frame_slot *slot,
                                                     const void *message,
                                                     size_t message_size,
                                                     char *err,
                                                     size_t err_size) {
    hasciicam_virtual_camera_pipe_frame header;
    const unsigned char *payload = NULL;
    size_t payload_size = 0;
    unsigned char *copy = NULL;

    if (slot == NULL) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "source frame slot is required");
        return 0;
    }
    if (!hasciicam_virtual_camera_pipe_decode_message(message,
                                                      message_size,
                                                      &header,
                                                      &payload,
                                                      &payload_size,
                                                      err,
                                                      err_size))
        return 0;
    copy = (unsigned char *)malloc(message_size);
    if (copy == NULL) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "unable to allocate source frame buffer");
        return 0;
    }
    memcpy(copy, message, message_size);
    free(slot->bytes);
    slot->bytes = copy;
    slot->bytes_size = message_size;
    slot->capacity = message_size;
    slot->sequence = header.sequence;
    slot->timestamp_100ns = header.timestamp_100ns;
    return 1;
}

int hasciicam_virtual_camera_source_make_black_message(const hasciicam_virtual_camera_source_config *config,
                                                       unsigned long long sequence,
                                                       unsigned long long timestamp_100ns,
                                                       void *bytes,
                                                       size_t bytes_size,
                                                       char *err,
                                                       size_t err_size) {
    hasciicam_virtual_camera_pipe_frame frame;
    unsigned char *payload;
    size_t payload_size;
    size_t i;

    if (config == NULL || bytes == NULL) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "source config and output buffer are required");
        return 0;
    }
    if (config->media_type_count == 0 ||
        config->media_types[0].pixel_format != HASCIICAM_VIRTUAL_CAMERA_PIXFMT_YUY2) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "black frame helper expects YUY2 media type");
        return 0;
    }
    hasciicam_virtual_camera_pipe_frame_init(&frame,
                                             HASCIICAM_VIRTUAL_CAMERA_PIXFMT_YUY2,
                                             config->request.width,
                                             config->request.height,
                                             config->media_types[0].stride_bytes,
                                             sequence,
                                             timestamp_100ns);
    payload_size = hasciicam_virtual_camera_pipe_frame_payload_size(&frame);
    if (payload_size == 0 || bytes_size < sizeof(frame) + payload_size) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "black frame buffer too small");
        return 0;
    }
    memcpy(bytes, &frame, sizeof(frame));
    payload = (unsigned char *)bytes + sizeof(frame);
    for (i = 0; i < payload_size; i += 4) {
        payload[i + 0] = 0x10;
        payload[i + 1] = 0x80;
        payload[i + 2] = 0x10;
        payload[i + 3] = 0x80;
    }
    return 1;
}

int hasciicam_virtual_camera_source_make_sample(const hasciicam_virtual_camera_source_config *config,
                                                const hasciicam_virtual_camera_source_frame_slot *slot,
                                                unsigned long long start_100ns,
                                                unsigned long long sequence,
                                                IMFSample **sample_out,
                                                char *err,
                                                size_t err_size) {
    hasciicam_virtual_camera_pipe_frame header;
    const unsigned char *payload = NULL;
    size_t payload_size = 0;
    size_t message_size = 0;
    unsigned char *message = NULL;
    IMFSample *sample = NULL;
    IMFMediaBuffer *buffer = NULL;
    BYTE *buffer_data = NULL;
    DWORD max_length = 0;
    DWORD current_length = 0;
    unsigned long long sample_time = 0;
    unsigned long long sample_duration = 0;
    HRESULT hr;

    if (sample_out == NULL) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "sample output is required");
        return 0;
    }
    *sample_out = NULL;
    if (config == NULL) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "source config is required");
        return 0;
    }
    if (config->media_type_count == 0) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "source config does not describe a media type");
        return 0;
    }

    if (slot != NULL && hasciicam_virtual_camera_source_frame_slot_has_message(slot)) {
        if (!hasciicam_virtual_camera_pipe_decode_message(slot->bytes,
                                                          slot->bytes_size,
                                                          &header,
                                                          &payload,
                                                          &payload_size,
                                                          err,
                                                          err_size))
            return 0;
        sample_time = slot->timestamp_100ns;
    } else {
        message_size = sizeof(hasciicam_virtual_camera_pipe_frame) + config->media_types[0].frame_bytes;
        message = (unsigned char *)malloc(message_size);
        if (message == NULL) {
            if (err != NULL && err_size > 0)
                snprintf(err, err_size, "unable to allocate black sample buffer");
            return 0;
        }
        if (!hasciicam_virtual_camera_source_make_black_message(config,
                                                                sequence,
                                                                start_100ns,
                                                                message,
                                                                message_size,
                                                                err,
                                                                err_size)) {
            free(message);
            return 0;
        }
        if (!hasciicam_virtual_camera_pipe_decode_message(message,
                                                          message_size,
                                                          &header,
                                                          &payload,
                                                          &payload_size,
                                                          err,
                                                          err_size)) {
            free(message);
            return 0;
        }
        sample_time = hasciicam_virtual_camera_source_sample_time_100ns(start_100ns,
                                                                         sequence,
                                                                         config->request.fps);
    }

    hr = MFCreateSample(&sample);
    if (FAILED(hr)) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "unable to allocate media sample");
        free(message);
        return 0;
    }
    hr = MFCreateMemoryBuffer((DWORD)payload_size, &buffer);
    if (FAILED(hr) || buffer == NULL) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "unable to allocate media buffer");
        if (sample != NULL)
            sample->Release();
        free(message);
        return 0;
    }
    hr = buffer->Lock(&buffer_data, &max_length, &current_length);
    if (SUCCEEDED(hr) && buffer_data != NULL && max_length >= payload_size) {
        memcpy(buffer_data, payload, payload_size);
        hr = buffer->Unlock();
    }
    if (SUCCEEDED(hr))
        hr = buffer->SetCurrentLength((DWORD)payload_size);
    if (SUCCEEDED(hr))
        hr = sample->AddBuffer(buffer);
    if (SUCCEEDED(hr))
        hr = sample->SetSampleTime((LONGLONG)sample_time);
    if (SUCCEEDED(hr)) {
        sample_duration = hasciicam_virtual_camera_source_sample_duration_100ns(config->request.fps);
        hr = sample->SetSampleDuration((LONGLONG)sample_duration);
    }
    buffer->Release();
    if (FAILED(hr)) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "unable to build media sample");
        sample->Release();
        free(message);
        return 0;
    }

    *sample_out = sample;
    free(message);
    return 1;
}

unsigned long long hasciicam_virtual_camera_source_sample_duration_100ns(int fps) {
    return hasciicam_virtual_camera_media_type_duration_100ns(fps);
}

unsigned long long hasciicam_virtual_camera_source_sample_time_100ns(unsigned long long start_100ns,
                                                                     unsigned long long sequence,
                                                                     int fps) {
    unsigned long long duration = hasciicam_virtual_camera_source_sample_duration_100ns(fps);
    return start_100ns + sequence * duration;
}

static HRESULT hasciicam_virtual_camera_source_build_mf_media_type(const hasciicam_virtual_camera_source_media_type *desc,
                                                                   IMFMediaType **out) {
    IMFMediaType *type = NULL;
    HRESULT hr;

    if (out == NULL || desc == NULL)
        return E_POINTER;
    *out = NULL;

    hr = MFCreateMediaType(&type);
    if (FAILED(hr))
        return hr;
    hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (SUCCEEDED(hr))
        hr = type->SetGUID(MF_MT_SUBTYPE, *desc->subtype);
    if (SUCCEEDED(hr))
        hr = MFSetAttributeSize(type, MF_MT_FRAME_SIZE, (UINT32)desc->width, (UINT32)desc->height);
    if (SUCCEEDED(hr))
        hr = MFSetAttributeRatio(type, MF_MT_FRAME_RATE, (UINT32)desc->fps, 1);
    if (SUCCEEDED(hr))
        hr = MFSetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (SUCCEEDED(hr))
        hr = type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (SUCCEEDED(hr))
        hr = type->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE);
    if (SUCCEEDED(hr))
        hr = type->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    if (SUCCEEDED(hr))
        hr = type->SetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32)desc->stride_bytes);
    if (SUCCEEDED(hr))
        hr = type->SetUINT32(MF_MT_SAMPLE_SIZE, (UINT32)desc->frame_bytes);
    if (SUCCEEDED(hr))
        hr = type->SetUINT32(MF_MT_AVG_BITRATE, (UINT32)desc->average_bitrate);
    if (FAILED(hr)) {
        type->Release();
        return hr;
    }

    *out = type;
    return S_OK;
}

static HRESULT hasciicam_virtual_camera_source_build_media_types(const hasciicam_virtual_camera_source_config *config,
                                                                 IMFMediaType **types,
                                                                 size_t type_count) {
    size_t i;
    HRESULT hr = S_OK;

    if (config == NULL || types == NULL)
        return E_POINTER;
    for (i = 0; i < type_count; ++i)
        types[i] = NULL;
    for (i = 0; i < type_count; ++i) {
        hr = hasciicam_virtual_camera_source_build_mf_media_type(&config->media_types[i], &types[i]);
        if (FAILED(hr))
            break;
    }
    if (FAILED(hr)) {
        while (i > 0) {
            --i;
            if (types[i] != NULL)
                types[i]->Release();
            types[i] = NULL;
        }
    }
    return hr;
}

class HasciiCamVirtualCameraStream : public IMFMediaStream2 {
public:
    HasciiCamVirtualCameraStream()
        : refcount_(1),
          stream_state_(MF_STREAM_STATE_STOPPED),
          event_queue_(NULL),
          descriptor_(NULL),
          type_handler_(NULL),
          attributes_(NULL),
          current_media_type_(NULL),
          sample_allocator_(NULL),
          source_(NULL),
          frame_lock_(NULL),
          next_sample_time_100ns_(0),
          sample_request_count_(0),
          sample_clock_started_(FALSE),
          shutdown_(FALSE) {
        InterlockedIncrement(&g_module_refcount);
    }

    ~HasciiCamVirtualCameraStream() {
        if (current_media_type_ != NULL)
            current_media_type_->Release();
        if (sample_allocator_ != NULL)
            sample_allocator_->Release();
        if (type_handler_ != NULL)
            type_handler_->Release();
        if (descriptor_ != NULL)
            descriptor_->Release();
        if (attributes_ != NULL)
            attributes_->Release();
        if (event_queue_ != NULL)
            event_queue_->Release();
        InterlockedDecrement(&g_module_refcount);
    }

    HRESULT Init(const hasciicam_virtual_camera_source_config *config,
                 const hasciicam_virtual_camera_source_frame_slot *frame_slot,
                 const hasciicam_virtual_camera_source_lifecycle *lifecycle,
                 PSRWLOCK frame_lock,
                 IMFMediaSource *source) {
        IMFMediaType *media_types[2] = { NULL, NULL };
        size_t media_type_count;
        HRESULT hr;

        if (config == NULL || frame_lock == NULL || source == NULL)
            return E_POINTER;
        config_ = *config;
        frame_slot_ = frame_slot;
        lifecycle_ = lifecycle;
        frame_lock_ = frame_lock;
        source_ = source;

        hr = MFCreateAttributes(&attributes_, 8);
        if (FAILED(hr))
            return hr;
        hr = attributes_->SetGUID(MF_DEVICESTREAM_STREAM_CATEGORY, PINNAME_VIDEO_CAPTURE);
        if (SUCCEEDED(hr))
            hr = attributes_->SetUINT32(MF_DEVICESTREAM_STREAM_ID, 0);
        if (SUCCEEDED(hr))
            hr = attributes_->SetUINT32(MF_DEVICESTREAM_FRAMESERVER_SHARED, TRUE);
        if (SUCCEEDED(hr))
            hr = attributes_->SetUINT32(MF_DEVICESTREAM_ATTRIBUTE_FRAMESOURCE_TYPES,
                                        MFFrameSourceTypes_Color);
        if (SUCCEEDED(hr))
            hr = MFCreateEventQueue(&event_queue_);
        if (FAILED(hr))
            return hr;

        media_type_count = config_.media_type_count;
        hr = hasciicam_virtual_camera_source_build_media_types(&config_, media_types, media_type_count);
        if (FAILED(hr))
            return hr;

        hr = MFCreateStreamDescriptor(0,
                                       (DWORD)media_type_count,
                                       media_types,
                                       &descriptor_);
        if (FAILED(hr))
        {
            while (media_type_count > 0) {
                --media_type_count;
                if (media_types[media_type_count] != NULL)
                    media_types[media_type_count]->Release();
            }
            return hr;
        }
        hr = descriptor_->SetGUID(MF_DEVICESTREAM_STREAM_CATEGORY, PINNAME_VIDEO_CAPTURE);
        if (SUCCEEDED(hr))
            hr = descriptor_->SetUINT32(MF_DEVICESTREAM_STREAM_ID, 0);
        if (SUCCEEDED(hr))
            hr = descriptor_->SetUINT32(MF_DEVICESTREAM_FRAMESERVER_SHARED, TRUE);
        if (SUCCEEDED(hr))
            hr = descriptor_->SetUINT32(MF_DEVICESTREAM_ATTRIBUTE_FRAMESOURCE_TYPES,
                                        MFFrameSourceTypes_Color);
        if (FAILED(hr)) {
            while (media_type_count > 0) {
                --media_type_count;
                if (media_types[media_type_count] != NULL)
                    media_types[media_type_count]->Release();
            }
            return hr;
        }

        hr = descriptor_->GetMediaTypeHandler(&type_handler_);
        if (FAILED(hr))
        {
            while (media_type_count > 0) {
                --media_type_count;
                if (media_types[media_type_count] != NULL)
                    media_types[media_type_count]->Release();
            }
            return hr;
        }

        hr = type_handler_->SetCurrentMediaType(media_types[0]);
        if (FAILED(hr))
        {
            while (media_type_count > 0) {
                --media_type_count;
                if (media_types[media_type_count] != NULL)
                    media_types[media_type_count]->Release();
            }
            return hr;
        }

        hr = type_handler_->GetCurrentMediaType(&current_media_type_);
        if (FAILED(hr))
        {
            while (media_type_count > 0) {
                --media_type_count;
                if (media_types[media_type_count] != NULL)
                    media_types[media_type_count]->Release();
            }
            return hr;
        }

        while (media_type_count > 0) {
            --media_type_count;
            if (media_types[media_type_count] != NULL)
                media_types[media_type_count]->Release();
        }

        return S_OK;
    }

    ULONG AddRef(void) {
        return (ULONG)InterlockedIncrement(&refcount_);
    }

    ULONG Release(void) {
        ULONG refcount = (ULONG)InterlockedDecrement(&refcount_);
        if (refcount == 0)
            delete this;
        return refcount;
    }

    HRESULT QueryInterface(REFIID riid, void **ppv) {
        if (ppv == NULL)
            return E_POINTER;
        *ppv = NULL;
        if (IsEqualIID(riid, IID_IUnknown) ||
            IsEqualIID(riid, IID_IMFMediaEventGenerator) ||
            IsEqualIID(riid, IID_IMFMediaStream) ||
            IsEqualIID(riid, IID_IMFMediaStream2)) {
            *ppv = static_cast<IMFMediaStream2 *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    HRESULT GetEvent(DWORD flags, IMFMediaEvent **event) {
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (event_queue_ == NULL)
            return E_UNEXPECTED;
        return event_queue_->GetEvent(flags, event);
    }

    HRESULT BeginGetEvent(IMFAsyncCallback *callback, IUnknown *state) {
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (event_queue_ == NULL)
            return E_UNEXPECTED;
        return event_queue_->BeginGetEvent(callback, state);
    }

    HRESULT EndGetEvent(IMFAsyncResult *result, IMFMediaEvent **event) {
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (event_queue_ == NULL)
            return E_UNEXPECTED;
        return event_queue_->EndGetEvent(result, event);
    }

    HRESULT QueueEvent(MediaEventType type, REFGUID extended_type, HRESULT status, const PROPVARIANT *value) {
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (event_queue_ == NULL)
            return E_UNEXPECTED;
        return event_queue_->QueueEventParamVar(type, extended_type, status, value);
    }

    HRESULT GetMediaSource(IMFMediaSource **media_source) {
        if (media_source == NULL)
            return E_POINTER;
        *media_source = NULL;
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (source_ == NULL)
            return E_UNEXPECTED;
        source_->AddRef();
        *media_source = source_;
        return S_OK;
    }

    HRESULT GetStreamDescriptor(IMFStreamDescriptor **stream_descriptor) {
        if (stream_descriptor == NULL)
            return E_POINTER;
        *stream_descriptor = NULL;
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (descriptor_ == NULL)
            return E_UNEXPECTED;
        descriptor_->AddRef();
        *stream_descriptor = descriptor_;
        return S_OK;
    }

    HRESULT RequestSample(IUnknown *token) {
        IMFSample *sample = NULL;
        IMFSample *allocated_sample = NULL;
        IMFMediaBuffer *source_buffer = NULL;
        IMFMediaBuffer *destination_buffer = NULL;
        IMF2DBuffer2 *destination_buffer_2d = NULL;
        BYTE *source_bytes = NULL;
        BYTE *destination_bytes = NULL;
        BYTE *destination_start = NULL;
        DWORD source_length = 0;
        DWORD destination_capacity = 0;
        LONG destination_pitch = 0;
        const char *failure_stage = "build source sample";
        int destination_locked = 0;
        unsigned long long start_100ns = 0ULL;
        unsigned long long sequence = 0ULL;
        unsigned long long sample_time;
        unsigned long long sample_duration;
        HRESULT hr;

        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (stream_state_ != MF_STREAM_STATE_RUNNING)
            return MF_E_INVALIDREQUEST;
        sample_request_count_++;
        if (sample_request_count_ == 1)
            hasciicam_virtual_camera_source_trace("first sample request received");
        AcquireSRWLockShared(frame_lock_);
        if (lifecycle_ != NULL) {
            start_100ns = lifecycle_->last_timestamp_100ns;
            sequence = lifecycle_->last_sequence;
        }
        if (frame_slot_ != NULL && hasciicam_virtual_camera_source_frame_slot_has_message(frame_slot_)) {
            start_100ns = frame_slot_->timestamp_100ns;
            sequence = frame_slot_->sequence;
        }
        hr = hasciicam_virtual_camera_source_make_sample(&config_,
                                                         frame_slot_,
                                                         start_100ns,
                                                         sequence,
                                                         &sample,
                                                         NULL,
                                                         0);
        ReleaseSRWLockShared(frame_lock_);
        if (FAILED(hr) || sample == NULL) {
            hasciicam_virtual_camera_source_trace(
                "sample failure stage=%s result=0x%08lx",
                failure_stage,
                (unsigned long)(FAILED(hr) ? hr : E_FAIL));
            return FAILED(hr) ? hr : E_FAIL;
        }
        if (sample_allocator_ != NULL) {
            failure_stage = "allocate provided sample";
            hr = sample_allocator_->AllocateSample(&allocated_sample);
            if (SUCCEEDED(hr)) {
                failure_stage = "get source buffer";
                hr = sample->ConvertToContiguousBuffer(&source_buffer);
            }
            if (SUCCEEDED(hr)) {
                failure_stage = "get destination buffer";
                hr = allocated_sample->GetBufferByIndex(0, &destination_buffer);
            }
            if (SUCCEEDED(hr)) {
                failure_stage = "lock source buffer";
                hr = source_buffer->Lock(&source_bytes, NULL, &source_length);
            }
            if (SUCCEEDED(hr)) {
                failure_stage = "query destination 2D buffer";
                hr = destination_buffer->QueryInterface(
                    IID_IMF2DBuffer2, (void **)&destination_buffer_2d);
            }
            if (SUCCEEDED(hr)) {
                failure_stage = "lock destination 2D buffer";
                hr = destination_buffer_2d->Lock2DSize(
                    MF2DBuffer_LockFlags_Write,
                    &destination_bytes,
                    &destination_pitch,
                    &destination_start,
                    &destination_capacity);
                if (SUCCEEDED(hr))
                    destination_locked = 1;
            }
            if (SUCCEEDED(hr)) {
                size_t row_bytes = (size_t)config_.media_types[0].stride_bytes;
                size_t required_size = row_bytes * (size_t)config_.request.height;
                int row;

                failure_stage = "copy destination rows";
                if (source_length < required_size ||
                    destination_capacity < required_size ||
                    destination_pitch == 0 ||
                    (size_t)(destination_pitch < 0 ? -destination_pitch : destination_pitch) < row_bytes) {
                    hr = MF_E_BUFFERTOOSMALL;
                } else {
                    for (row = 0; row < config_.request.height; ++row) {
                        BYTE *destination_row =
                            destination_bytes + (ptrdiff_t)row * destination_pitch;
                        memcpy(destination_row,
                               source_bytes + (size_t)row * row_bytes,
                               row_bytes);
                    }
                }
            }
            if (destination_locked)
                destination_buffer_2d->Unlock2D();
            if (source_bytes != NULL)
                source_buffer->Unlock();
            if (SUCCEEDED(hr)) {
                failure_stage = "set destination length";
                hr = destination_buffer->SetCurrentLength(source_length);
            }
            if (source_buffer != NULL)
                source_buffer->Release();
            if (destination_buffer_2d != NULL)
                destination_buffer_2d->Release();
            if (destination_buffer != NULL)
                destination_buffer->Release();
            sample->Release();
            sample = allocated_sample;
            allocated_sample = NULL;
            if (FAILED(hr)) {
                if (sample != NULL)
                    sample->Release();
                hasciicam_virtual_camera_source_trace(
                    "sample failure stage=%s result=0x%08lx source_bytes=%lu destination_bytes=%lu pitch=%ld",
                    failure_stage,
                    (unsigned long)hr,
                    (unsigned long)source_length,
                    (unsigned long)destination_capacity,
                    (long)destination_pitch);
                return hr;
            }
        }
        sample_duration = hasciicam_virtual_camera_source_sample_duration_100ns(config_.request.fps);
        sample_time = (unsigned long long)MFGetSystemTime();
        if (!sample_clock_started_) {
            next_sample_time_100ns_ = sample_time;
            sample_clock_started_ = TRUE;
        }
        if (sample_time < next_sample_time_100ns_)
            sample_time = next_sample_time_100ns_;
        hr = sample->SetSampleTime((LONGLONG)sample_time);
        if (SUCCEEDED(hr))
            hr = sample->SetSampleDuration((LONGLONG)sample_duration);
        if (FAILED(hr)) {
            sample->Release();
            return hr;
        }
        next_sample_time_100ns_ = sample_time + sample_duration;
        if (sample_request_count_ == 1 || sample_request_count_ % 300 == 0) {
            hasciicam_virtual_camera_source_trace(
                "sample request=%llu frame_sequence=%llu time=%llu bytes=%llu",
                sample_request_count_,
                sequence,
                sample_time,
                (unsigned long long)config_.media_types[0].frame_bytes);
        }
        if (token != NULL) {
            hr = sample->SetUnknown(MFSampleExtension_Token, token);
            if (FAILED(hr)) {
                sample->Release();
                return hr;
            }
        }
        hr = event_queue_->QueueEventParamUnk(MEMediaSample, GUID_NULL, S_OK, sample);
        sample->Release();
        return hr;
    }

    HRESULT GetMediaTypeHandler(IMFMediaTypeHandler **handler) {
        if (handler == NULL)
            return E_POINTER;
        *handler = NULL;
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (type_handler_ == NULL)
            return E_UNEXPECTED;
        type_handler_->AddRef();
        *handler = type_handler_;
        return S_OK;
    }

    HRESULT SetStreamState(MF_STREAM_STATE state) {
        HRESULT hr;

        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (state == MF_STREAM_STATE_RUNNING && sample_allocator_ != NULL) {
            if (current_media_type_ == NULL)
                return MF_E_NOT_INITIALIZED;
            hr = sample_allocator_->InitializeSampleAllocator(10, current_media_type_);
            hasciicam_virtual_camera_source_trace(
                "sample allocator initialized result=0x%08lx",
                (unsigned long)hr);
            if (FAILED(hr))
                return hr;
        }
        stream_state_ = state;
        if (state == MF_STREAM_STATE_RUNNING) {
            next_sample_time_100ns_ = 0;
            sample_clock_started_ = FALSE;
        }
        return S_OK;
    }

    HRESULT GetStreamState(MF_STREAM_STATE *state) {
        if (state == NULL)
            return E_POINTER;
        if (shutdown_)
            return MF_E_SHUTDOWN;
        *state = stream_state_;
        return S_OK;
    }

    HRESULT GetAttributes(IMFAttributes **attributes) {
        if (attributes == NULL)
            return E_POINTER;
        *attributes = NULL;
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (attributes_ == NULL)
            return E_UNEXPECTED;
        attributes_->AddRef();
        *attributes = attributes_;
        return S_OK;
    }

    HRESULT SetCurrentMediaType(IMFMediaType *media_type) {
        IMFMediaType *selected_type = NULL;
        HRESULT hr;

        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (type_handler_ == NULL)
            return E_UNEXPECTED;
        if (media_type == NULL)
            return E_POINTER;
        hr = type_handler_->SetCurrentMediaType(media_type);
        if (SUCCEEDED(hr))
            hr = type_handler_->GetCurrentMediaType(&selected_type);
        if (FAILED(hr))
            return hr;
        if (current_media_type_ != NULL)
            current_media_type_->Release();
        current_media_type_ = selected_type;
        return S_OK;
    }

    HRESULT GetCurrentMediaType(IMFMediaType **media_type) {
        if (media_type == NULL)
            return E_POINTER;
        *media_type = NULL;
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (type_handler_ == NULL)
            return E_UNEXPECTED;
        return type_handler_->GetCurrentMediaType(media_type);
    }

    HRESULT SetSampleAllocator(IMFVideoSampleAllocator *allocator) {
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (sample_allocator_ != NULL)
            sample_allocator_->Release();
        sample_allocator_ = allocator;
        if (sample_allocator_ != NULL)
            sample_allocator_->AddRef();
        hasciicam_virtual_camera_source_trace(
            "sample allocator %s",
            sample_allocator_ != NULL ? "set" : "cleared");
        return S_OK;
    }

    HRESULT Shutdown(void) {
        shutdown_ = TRUE;
        stream_state_ = MF_STREAM_STATE_STOPPED;
        source_ = NULL;
        if (event_queue_ != NULL) {
            event_queue_->Shutdown();
            event_queue_->Release();
            event_queue_ = NULL;
        }
        if (current_media_type_ != NULL) {
            current_media_type_->Release();
            current_media_type_ = NULL;
        }
        if (sample_allocator_ != NULL) {
            sample_allocator_->Release();
            sample_allocator_ = NULL;
        }
        if (type_handler_ != NULL) {
            type_handler_->Release();
            type_handler_ = NULL;
        }
        if (descriptor_ != NULL) {
            descriptor_->Release();
            descriptor_ = NULL;
        }
        if (attributes_ != NULL) {
            attributes_->Release();
            attributes_ = NULL;
        }
        return S_OK;
    }

private:
    LONG refcount_;
    MF_STREAM_STATE stream_state_;
    IMFMediaEventQueue *event_queue_;
    IMFStreamDescriptor *descriptor_;
    IMFMediaTypeHandler *type_handler_;
    IMFAttributes *attributes_;
    IMFMediaType *current_media_type_;
    IMFVideoSampleAllocator *sample_allocator_;
    IMFMediaSource *source_;
    hasciicam_virtual_camera_source_config config_;
    const hasciicam_virtual_camera_source_frame_slot *frame_slot_;
    const hasciicam_virtual_camera_source_lifecycle *lifecycle_;
    PSRWLOCK frame_lock_;
    unsigned long long next_sample_time_100ns_;
    unsigned long long sample_request_count_;
    BOOL sample_clock_started_;
    BOOL shutdown_;
};

class HasciiCamVirtualCameraSource : public IMFMediaSourceEx,
                                    public IMFGetService,
                                    public IKsControl,
                                    public IMFSampleAllocatorControl {
public:
    HasciiCamVirtualCameraSource()
        : refcount_(1),
          event_queue_(NULL),
          attributes_(NULL),
          stream_(NULL),
          presentation_descriptor_(NULL),
          stream_descriptor_(NULL),
          reader_thread_(NULL),
          reader_stop_event_(NULL),
          stream_announced_(FALSE),
          reader_frame_count_(0),
          shutdown_(FALSE) {
        InitializeSRWLock(&frame_lock_);
        hasciicam_virtual_camera_source_frame_slot_init(&frame_slot_);
        InterlockedIncrement(&g_module_refcount);
    }

    ~HasciiCamVirtualCameraSource() {
        StopReaderThread();
        if (presentation_descriptor_ != NULL)
            presentation_descriptor_->Release();
        if (stream_ != NULL)
            stream_->Release();
        if (attributes_ != NULL)
            attributes_->Release();
        if (event_queue_ != NULL)
            event_queue_->Release();
        hasciicam_virtual_camera_source_frame_slot_close(&frame_slot_);
        InterlockedDecrement(&g_module_refcount);
    }

    HRESULT Init(const hasciicam_virtual_camera_source_config *config) {
        HRESULT hr;

        if (config == NULL)
            return E_POINTER;
        config_ = *config;
        lifecycle_.state = HASCIICAM_VIRTUAL_CAMERA_SOURCE_STATE_CREATED;
        lifecycle_.config = *config;

        hr = MFCreateAttributes(&attributes_, 8);
        if (FAILED(hr))
            return hr;
        hr = attributes_->SetGUID(MF_DEVICESTREAM_STREAM_CATEGORY, PINNAME_VIDEO_CAPTURE);
        if (SUCCEEDED(hr))
            hr = attributes_->SetUINT32(MF_DEVICESTREAM_STREAM_ID, 0);
        if (SUCCEEDED(hr))
            hr = attributes_->SetUINT32(MF_DEVICESTREAM_FRAMESERVER_SHARED, TRUE);
        if (SUCCEEDED(hr))
            hr = MFCreateEventQueue(&event_queue_);
        if (FAILED(hr))
            return hr;

        stream_ = new (std::nothrow) HasciiCamVirtualCameraStream();
        if (stream_ == NULL)
            return E_OUTOFMEMORY;
        hr = stream_->Init(&config_,
                           &frame_slot_,
                           &lifecycle_,
                           &frame_lock_,
                           static_cast<IMFMediaSource *>(this));
        if (FAILED(hr))
            return hr;

        hr = stream_->GetStreamDescriptor(&stream_descriptor_);
        if (FAILED(hr))
            return hr;

        hr = MFCreatePresentationDescriptor(1, &stream_descriptor_, &presentation_descriptor_);
        if (FAILED(hr))
            return hr;

        hasciicam_virtual_camera_source_trace(
            "source initialized pipe=%s size=%dx%d fps=%d",
            config_.pipe_name,
            config_.request.width,
            config_.request.height,
            config_.request.fps);
        return S_OK;
    }

    ULONG AddRef(void) {
        return (ULONG)InterlockedIncrement(&refcount_);
    }

    ULONG Release(void) {
        ULONG refcount = (ULONG)InterlockedDecrement(&refcount_);
        if (refcount == 0)
            delete this;
        return refcount;
    }

    HRESULT QueryInterface(REFIID riid, void **ppv) {
        if (ppv == NULL)
            return E_POINTER;
        *ppv = NULL;
        if (IsEqualIID(riid, IID_IUnknown) ||
            IsEqualIID(riid, IID_IMFMediaEventGenerator) ||
            IsEqualIID(riid, IID_IMFMediaSource) ||
            IsEqualIID(riid, IID_IMFMediaSourceEx)) {
            *ppv = static_cast<IMFMediaSourceEx *>(this);
            AddRef();
            return S_OK;
        }
        if (IsEqualIID(riid, IID_IMFGetService)) {
            *ppv = static_cast<IMFGetService *>(this);
            AddRef();
            return S_OK;
        }
        if (IsEqualIID(riid, __uuidof(IKsControl))) {
            *ppv = static_cast<IKsControl *>(this);
            AddRef();
            return S_OK;
        }
        if (IsEqualIID(riid, IID_IMFSampleAllocatorControl)) {
            *ppv = static_cast<IMFSampleAllocatorControl *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    HRESULT GetEvent(DWORD flags, IMFMediaEvent **event) {
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (event_queue_ == NULL)
            return E_UNEXPECTED;
        return event_queue_->GetEvent(flags, event);
    }

    HRESULT BeginGetEvent(IMFAsyncCallback *callback, IUnknown *state) {
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (event_queue_ == NULL)
            return E_UNEXPECTED;
        return event_queue_->BeginGetEvent(callback, state);
    }

    HRESULT EndGetEvent(IMFAsyncResult *result, IMFMediaEvent **event) {
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (event_queue_ == NULL)
            return E_UNEXPECTED;
        return event_queue_->EndGetEvent(result, event);
    }

    HRESULT QueueEvent(MediaEventType type, REFGUID extended_type, HRESULT status, const PROPVARIANT *value) {
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (event_queue_ == NULL)
            return E_UNEXPECTED;
        return event_queue_->QueueEventParamVar(type, extended_type, status, value);
    }

    HRESULT GetCharacteristics(DWORD *characteristics) {
        if (characteristics == NULL)
            return E_POINTER;
        if (shutdown_)
            return MF_E_SHUTDOWN;
        *characteristics = MFMEDIASOURCE_IS_LIVE | MFMEDIASOURCE_DOES_NOT_USE_NETWORK;
        return S_OK;
    }

    HRESULT CreatePresentationDescriptor(IMFPresentationDescriptor **descriptor) {
        if (descriptor == NULL)
            return E_POINTER;
        *descriptor = NULL;
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (presentation_descriptor_ == NULL)
            return E_UNEXPECTED;
        presentation_descriptor_->AddRef();
        *descriptor = presentation_descriptor_;
        return S_OK;
    }

    HRESULT Start(IMFPresentationDescriptor *presentation_descriptor,
                  const GUID *time_format,
                  const PROPVARIANT *start_position) {
        IMFStreamDescriptor *stream_descriptor = NULL;
        IMFMediaTypeHandler *type_handler = NULL;
        IMFMediaType *media_type = NULL;
        PROPVARIANT start_value;
        MediaEventType stream_event;
        BOOL selected = FALSE;
        HRESULT hr;

        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (presentation_descriptor == NULL || start_position == NULL)
            return E_POINTER;
        if (time_format != NULL && !IsEqualGUID(*time_format, GUID_NULL))
            return MF_E_UNSUPPORTED_TIME_FORMAT;
        if (start_position->vt != VT_EMPTY && start_position->vt != VT_I8)
            return MF_E_UNSUPPORTED_TIME_FORMAT;
        hr = presentation_descriptor->GetStreamDescriptorByIndex(
            0, &selected, &stream_descriptor);
        if (FAILED(hr))
            return hr;
        if (!selected) {
            stream_descriptor->Release();
            return MF_E_INVALIDREQUEST;
        }
        hr = stream_descriptor->GetMediaTypeHandler(&type_handler);
        if (SUCCEEDED(hr))
            hr = type_handler->GetCurrentMediaType(&media_type);
        if (SUCCEEDED(hr))
            hr = stream_->SetCurrentMediaType(media_type);
        if (media_type != NULL)
            media_type->Release();
        if (type_handler != NULL)
            type_handler->Release();
        stream_descriptor->Release();
        if (FAILED(hr))
            return hr;
        if (lifecycle_.state == HASCIICAM_VIRTUAL_CAMERA_SOURCE_STATE_STARTED)
            return MF_E_INVALIDREQUEST;
        if (!hasciicam_virtual_camera_source_lifecycle_start(&lifecycle_, NULL, 0))
            return MF_E_INVALIDREQUEST;
        if (stream_ == NULL)
            return E_UNEXPECTED;
        hr = presentation_descriptor_->SelectStream(0);
        if (FAILED(hr))
            return hr;
        hr = stream_->SetStreamState(MF_STREAM_STATE_RUNNING);
        if (FAILED(hr))
            return hr;
        hr = StartReaderThread();
        if (FAILED(hr))
            return hr;

        stream_event = stream_announced_ ? MEUpdatedStream : MENewStream;
        hr = event_queue_->QueueEventParamUnk(
            stream_event, GUID_NULL, S_OK, static_cast<IMFMediaStream *>(stream_));
        if (FAILED(hr))
            return hr;
        stream_announced_ = TRUE;

        PropVariantInit(&start_value);
        start_value.vt = VT_I8;
        start_value.hVal.QuadPart = MFGetSystemTime();
        hr = event_queue_->QueueEventParamVar(
            MESourceStarted, GUID_NULL, S_OK, &start_value);
        if (SUCCEEDED(hr))
            hr = stream_->QueueEvent(MEStreamStarted, GUID_NULL, S_OK, NULL);
        PropVariantClear(&start_value);
        hasciicam_virtual_camera_source_trace(
            "source start selected=%d result=0x%08lx",
            selected,
            (unsigned long)hr);
        return hr;
    }

    HRESULT Stop(void) {
        HRESULT hr;

        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (!hasciicam_virtual_camera_source_lifecycle_stop(&lifecycle_, NULL, 0))
            return MF_E_INVALIDREQUEST;
        if (stream_ == NULL)
            return E_UNEXPECTED;
        hr = stream_->SetStreamState(MF_STREAM_STATE_STOPPED);
        if (SUCCEEDED(hr))
            hr = stream_->QueueEvent(MEStreamStopped, GUID_NULL, S_OK, NULL);
        if (SUCCEEDED(hr))
            hr = event_queue_->QueueEventParamVar(MESourceStopped, GUID_NULL, S_OK, NULL);
        return hr;
    }

    HRESULT Pause(void) {
        return MF_E_INVALIDREQUEST;
    }

    HRESULT Shutdown(void) {
        shutdown_ = TRUE;
        StopReaderThread();
        if (stream_ != NULL)
            stream_->Shutdown();
        if (presentation_descriptor_ != NULL) {
            presentation_descriptor_->Release();
            presentation_descriptor_ = NULL;
        }
        if (stream_descriptor_ != NULL) {
            stream_descriptor_->Release();
            stream_descriptor_ = NULL;
        }
        if (attributes_ != NULL) {
            attributes_->Release();
            attributes_ = NULL;
        }
        if (event_queue_ != NULL) {
            event_queue_->Shutdown();
            event_queue_->Release();
            event_queue_ = NULL;
        }
        hasciicam_virtual_camera_source_lifecycle_shutdown(&lifecycle_, NULL, 0);
        return S_OK;
    }

    HRESULT GetSourceAttributes(IMFAttributes **attributes) {
        if (attributes == NULL)
            return E_POINTER;
        *attributes = NULL;
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (attributes_ == NULL)
            return E_UNEXPECTED;
        attributes_->AddRef();
        *attributes = attributes_;
        return S_OK;
    }

    HRESULT GetStreamAttributes(DWORD stream_id, IMFAttributes **attributes) {
        if (attributes == NULL)
            return E_POINTER;
        *attributes = NULL;
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (stream_id != 0)
            return MF_E_INVALIDSTREAMNUMBER;
        if (stream_ == NULL)
            return E_UNEXPECTED;
        return stream_->GetAttributes(attributes);
    }

    HRESULT SetD3DManager(IUnknown *manager) {
        (void)manager;
        return S_OK;
    }

    HRESULT SetMediaType(DWORD stream_id, IMFMediaType *media_type) {
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (stream_id != 0)
            return MF_E_INVALIDSTREAMNUMBER;
        if (stream_ == NULL)
            return E_UNEXPECTED;
        return stream_->SetCurrentMediaType(media_type);
    }

    HRESULT GetService(REFGUID service, REFIID riid, LPVOID *object) {
        (void)service;
        (void)riid;
        if (object == NULL)
            return E_POINTER;
        *object = NULL;
        return MF_E_UNSUPPORTED_SERVICE;
    }

    HRESULT SetDefaultAllocator(DWORD output_stream_id, IUnknown *allocator) {
        IMFVideoSampleAllocator *video_allocator = NULL;
        HRESULT hr;

        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (output_stream_id != 0)
            return MF_E_INVALIDSTREAMNUMBER;
        if (allocator == NULL)
            return E_POINTER;
        hr = allocator->QueryInterface(
            IID_IMFVideoSampleAllocator, (void **)&video_allocator);
        if (FAILED(hr))
            return hr;
        hr = stream_->SetSampleAllocator(video_allocator);
        video_allocator->Release();
        return hr;
    }

    HRESULT GetAllocatorUsage(DWORD output_stream_id,
                              DWORD *input_stream_id,
                              MFSampleAllocatorUsage *usage) {
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (output_stream_id != 0)
            return MF_E_INVALIDSTREAMNUMBER;
        if (input_stream_id == NULL || usage == NULL)
            return E_POINTER;
        *input_stream_id = output_stream_id;
        *usage = MFSampleAllocatorUsage_UsesProvidedAllocator;
        return S_OK;
    }

    HRESULT KsProperty(PKSPROPERTY property,
                       ULONG property_length,
                       LPVOID data,
                       ULONG data_length,
                       ULONG *bytes_returned) {
        (void)property;
        (void)property_length;
        (void)data;
        (void)data_length;
        if (bytes_returned != NULL)
            *bytes_returned = 0;
        return E_NOTIMPL;
    }

    HRESULT KsMethod(PKSMETHOD method,
                     ULONG method_length,
                     LPVOID data,
                     ULONG data_length,
                     ULONG *bytes_returned) {
        (void)method;
        (void)method_length;
        (void)data;
        (void)data_length;
        if (bytes_returned != NULL)
            *bytes_returned = 0;
        return E_NOTIMPL;
    }

    HRESULT KsEvent(PKSEVENT event,
                    ULONG event_length,
                    LPVOID data,
                    ULONG data_length,
                    ULONG *bytes_returned) {
        (void)event;
        (void)event_length;
        (void)data;
        (void)data_length;
        if (bytes_returned != NULL)
            *bytes_returned = 0;
        return E_NOTIMPL;
    }

    HRESULT GetStreamDescriptor(IMFStreamDescriptor **stream_descriptor) {
        if (stream_descriptor == NULL)
            return E_POINTER;
        *stream_descriptor = NULL;
        if (shutdown_)
            return MF_E_SHUTDOWN;
        if (stream_descriptor_ == NULL)
            return E_UNEXPECTED;
        stream_descriptor_->AddRef();
        *stream_descriptor = stream_descriptor_;
        return S_OK;
    }

    HRESULT StartReaderThread(void) {
        if (reader_stop_event_ == NULL) {
            reader_stop_event_ = CreateEventW(NULL, TRUE, FALSE, NULL);
            if (reader_stop_event_ == NULL)
                return HRESULT_FROM_WIN32(GetLastError());
        }
        if (reader_thread_ != NULL)
            return S_OK;
        ResetEvent(reader_stop_event_);
        reader_thread_ = CreateThread(NULL, 0, &HasciiCamVirtualCameraSource::ReaderThreadProc, this, 0, NULL);
        if (reader_thread_ == NULL)
            return HRESULT_FROM_WIN32(GetLastError());
        return S_OK;
    }

    void StopReaderThread(void) {
        if (reader_stop_event_ != NULL)
            SetEvent(reader_stop_event_);
        if (reader_thread_ != NULL) {
            CancelSynchronousIo(reader_thread_);
            WaitForSingleObject(reader_thread_, INFINITE);
            CloseHandle(reader_thread_);
            reader_thread_ = NULL;
        }
        if (reader_stop_event_ != NULL) {
            CloseHandle(reader_stop_event_);
            reader_stop_event_ = NULL;
        }
    }

    static DWORD WINAPI ReaderThreadProc(LPVOID param) {
        HasciiCamVirtualCameraSource *self = (HasciiCamVirtualCameraSource *)param;
        hasciicam_virtual_camera_source_frame_slot incoming;
        char err[128];

        if (self == NULL)
            return 0;
        hasciicam_virtual_camera_source_frame_slot_init(&incoming);
        while (WaitForSingleObject(self->reader_stop_event_, 0) != WAIT_OBJECT_0) {
            if (hasciicam_virtual_camera_source_read_pipe_message(&self->config_,
                                                                  self->config_.pipe_name,
                                                                  &incoming,
                                                                  250,
                                                                  err,
                                                                  sizeof(err))) {
                AcquireSRWLockExclusive(&self->frame_lock_);
                {
                    hasciicam_virtual_camera_source_frame_slot previous = self->frame_slot_;
                    self->frame_slot_ = incoming;
                    incoming = previous;
                }
                self->lifecycle_.last_sequence = self->frame_slot_.sequence;
                self->lifecycle_.last_timestamp_100ns = self->frame_slot_.timestamp_100ns;
                self->reader_frame_count_++;
                ReleaseSRWLockExclusive(&self->frame_lock_);
                if (self->reader_frame_count_ == 1 ||
                    self->reader_frame_count_ % 300 == 0) {
                    hasciicam_virtual_camera_source_trace(
                        "pipe frame=%llu sequence=%llu timestamp=%llu",
                        self->reader_frame_count_,
                        self->lifecycle_.last_sequence,
                        self->lifecycle_.last_timestamp_100ns);
                }
            }
        }
        hasciicam_virtual_camera_source_frame_slot_close(&incoming);
        return 0;
    }

private:
    LONG refcount_;
    IMFMediaEventQueue *event_queue_;
    IMFAttributes *attributes_;
    HasciiCamVirtualCameraStream *stream_;
    IMFPresentationDescriptor *presentation_descriptor_;
    IMFStreamDescriptor *stream_descriptor_;
    HANDLE reader_thread_;
    HANDLE reader_stop_event_;
    BOOL stream_announced_;
    unsigned long long reader_frame_count_;
    SRWLOCK frame_lock_;
    hasciicam_virtual_camera_source_config config_;
    hasciicam_virtual_camera_source_lifecycle lifecycle_;
    hasciicam_virtual_camera_source_frame_slot frame_slot_;
    BOOL shutdown_;
};

class HasciiCamVirtualCameraActivate : public IMFActivate {
public:
    HasciiCamVirtualCameraActivate()
        : refcount_(1),
          attributes_(NULL),
          source_(NULL) {
        InterlockedIncrement(&g_module_refcount);
    }

    ~HasciiCamVirtualCameraActivate() {
        if (source_ != NULL)
            source_->Release();
        if (attributes_ != NULL)
            attributes_->Release();
        InterlockedDecrement(&g_module_refcount);
    }

    HRESULT Init(void) {
        return MFCreateAttributes(&attributes_, 4);
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
        if (ppv == NULL)
            return E_POINTER;
        *ppv = NULL;
        if (IsEqualIID(riid, IID_IUnknown) ||
            IsEqualIID(riid, IID_IMFAttributes) ||
            IsEqualIID(riid, IID_IMFActivate)) {
            *ppv = static_cast<IMFActivate *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef(void) override {
        return (ULONG)InterlockedIncrement(&refcount_);
    }

    ULONG STDMETHODCALLTYPE Release(void) override {
        ULONG refcount = (ULONG)InterlockedDecrement(&refcount_);
        if (refcount == 0)
            delete this;
        return refcount;
    }

    HRESULT STDMETHODCALLTYPE ActivateObject(REFIID riid, void **ppv) override {
        hasciicam_virtual_camera_request request;
        hasciicam_virtual_camera_source_config config;
        HRESULT hr;

        if (ppv == NULL)
            return E_POINTER;
        *ppv = NULL;
        if (source_ == NULL) {
            hasciicam_virtual_camera_source_trace("activate object requested");
            hasciicam_virtual_camera_request_init(&request);
            request.enabled = 1;
            request.width = 1280;
            request.height = 720;
            request.fps = 30;
            if (!hasciicam_virtual_camera_source_config_prepare(&request, &config, NULL, 0))
                return E_FAIL;

            source_ = new (std::nothrow) HasciiCamVirtualCameraSource();
            if (source_ == NULL)
                return E_OUTOFMEMORY;
            hr = source_->Init(&config);
            if (FAILED(hr)) {
                hasciicam_virtual_camera_source_trace(
                    "source initialization failed result=0x%08lx",
                    (unsigned long)hr);
                source_->Release();
                source_ = NULL;
                return hr;
            }
        }
        hasciicam_virtual_camera_source_trace("activate object ready");
        return source_->QueryInterface(riid, ppv);
    }

    HRESULT STDMETHODCALLTYPE ShutdownObject(void) override {
        if (source_ == NULL)
            return S_OK;
        return source_->Shutdown();
    }

    HRESULT STDMETHODCALLTYPE DetachObject(void) override {
        if (source_ != NULL) {
            source_->Release();
            source_ = NULL;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetItem(REFGUID key, PROPVARIANT *value) override {
        return attributes_->GetItem(key, value);
    }
    HRESULT STDMETHODCALLTYPE GetItemType(REFGUID key, MF_ATTRIBUTE_TYPE *type) override {
        return attributes_->GetItemType(key, type);
    }
    HRESULT STDMETHODCALLTYPE CompareItem(REFGUID key, REFPROPVARIANT value, BOOL *result) override {
        return attributes_->CompareItem(key, value, result);
    }
    HRESULT STDMETHODCALLTYPE Compare(IMFAttributes *theirs,
                                      MF_ATTRIBUTES_MATCH_TYPE match_type,
                                      BOOL *result) override {
        return attributes_->Compare(theirs, match_type, result);
    }
    HRESULT STDMETHODCALLTYPE GetUINT32(REFGUID key, UINT32 *value) override {
        return attributes_->GetUINT32(key, value);
    }
    HRESULT STDMETHODCALLTYPE GetUINT64(REFGUID key, UINT64 *value) override {
        return attributes_->GetUINT64(key, value);
    }
    HRESULT STDMETHODCALLTYPE GetDouble(REFGUID key, double *value) override {
        return attributes_->GetDouble(key, value);
    }
    HRESULT STDMETHODCALLTYPE GetGUID(REFGUID key, GUID *value) override {
        return attributes_->GetGUID(key, value);
    }
    HRESULT STDMETHODCALLTYPE GetStringLength(REFGUID key, UINT32 *length) override {
        return attributes_->GetStringLength(key, length);
    }
    HRESULT STDMETHODCALLTYPE GetString(REFGUID key,
                                        LPWSTR value,
                                        UINT32 value_size,
                                        UINT32 *length) override {
        return attributes_->GetString(key, value, value_size, length);
    }
    HRESULT STDMETHODCALLTYPE GetAllocatedString(REFGUID key,
                                                 LPWSTR *value,
                                                 UINT32 *length) override {
        return attributes_->GetAllocatedString(key, value, length);
    }
    HRESULT STDMETHODCALLTYPE GetBlobSize(REFGUID key, UINT32 *size) override {
        return attributes_->GetBlobSize(key, size);
    }
    HRESULT STDMETHODCALLTYPE GetBlob(REFGUID key,
                                      UINT8 *buffer,
                                      UINT32 buffer_size,
                                      UINT32 *size) override {
        return attributes_->GetBlob(key, buffer, buffer_size, size);
    }
    HRESULT STDMETHODCALLTYPE GetAllocatedBlob(REFGUID key,
                                               UINT8 **buffer,
                                               UINT32 *size) override {
        return attributes_->GetAllocatedBlob(key, buffer, size);
    }
    HRESULT STDMETHODCALLTYPE GetUnknown(REFGUID key, REFIID riid, LPVOID *ppv) override {
        return attributes_->GetUnknown(key, riid, ppv);
    }
    HRESULT STDMETHODCALLTYPE SetItem(REFGUID key, REFPROPVARIANT value) override {
        return attributes_->SetItem(key, value);
    }
    HRESULT STDMETHODCALLTYPE DeleteItem(REFGUID key) override {
        return attributes_->DeleteItem(key);
    }
    HRESULT STDMETHODCALLTYPE DeleteAllItems(void) override {
        return attributes_->DeleteAllItems();
    }
    HRESULT STDMETHODCALLTYPE SetUINT32(REFGUID key, UINT32 value) override {
        return attributes_->SetUINT32(key, value);
    }
    HRESULT STDMETHODCALLTYPE SetUINT64(REFGUID key, UINT64 value) override {
        return attributes_->SetUINT64(key, value);
    }
    HRESULT STDMETHODCALLTYPE SetDouble(REFGUID key, double value) override {
        return attributes_->SetDouble(key, value);
    }
    HRESULT STDMETHODCALLTYPE SetGUID(REFGUID key, REFGUID value) override {
        return attributes_->SetGUID(key, value);
    }
    HRESULT STDMETHODCALLTYPE SetString(REFGUID key, LPCWSTR value) override {
        return attributes_->SetString(key, value);
    }
    HRESULT STDMETHODCALLTYPE SetBlob(REFGUID key,
                                      const UINT8 *buffer,
                                      UINT32 buffer_size) override {
        return attributes_->SetBlob(key, buffer, buffer_size);
    }
    HRESULT STDMETHODCALLTYPE SetUnknown(REFGUID key, IUnknown *value) override {
        return attributes_->SetUnknown(key, value);
    }
    HRESULT STDMETHODCALLTYPE LockStore(void) override {
        return attributes_->LockStore();
    }
    HRESULT STDMETHODCALLTYPE UnlockStore(void) override {
        return attributes_->UnlockStore();
    }
    HRESULT STDMETHODCALLTYPE GetCount(UINT32 *count) override {
        return attributes_->GetCount(count);
    }
    HRESULT STDMETHODCALLTYPE GetItemByIndex(UINT32 index,
                                             GUID *key,
                                             PROPVARIANT *value) override {
        return attributes_->GetItemByIndex(index, key, value);
    }
    HRESULT STDMETHODCALLTYPE CopyAllItems(IMFAttributes *destination) override {
        return attributes_->CopyAllItems(destination);
    }

private:
    LONG refcount_;
    IMFAttributes *attributes_;
    HasciiCamVirtualCameraSource *source_;
};

class HasciiCamVirtualCameraClassFactory : public IClassFactory {
public:
    HasciiCamVirtualCameraClassFactory() : refcount_(1) {
        InterlockedIncrement(&g_module_refcount);
    }

    ~HasciiCamVirtualCameraClassFactory() {
        InterlockedDecrement(&g_module_refcount);
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
        if (ppv == NULL)
            return E_POINTER;
        *ppv = NULL;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppv = static_cast<IClassFactory *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef(void) override {
        return (ULONG)InterlockedIncrement(&refcount_);
    }

    ULONG STDMETHODCALLTYPE Release(void) override {
        ULONG refcount = (ULONG)InterlockedDecrement(&refcount_);
        if (refcount == 0) {
            delete this;
        }
        return refcount;
    }

    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *outer, REFIID riid, void **ppv) override {
        HasciiCamVirtualCameraActivate *activate;
        HRESULT hr;

        if (ppv != NULL)
            *ppv = NULL;
        if (ppv == NULL)
            return E_POINTER;
        if (outer != NULL)
            return CLASS_E_NOAGGREGATION;
        activate = new (std::nothrow) HasciiCamVirtualCameraActivate();
        if (activate == NULL)
            return E_OUTOFMEMORY;
        hr = activate->Init();
        if (SUCCEEDED(hr))
            hr = activate->QueryInterface(riid, ppv);
        activate->Release();
        return hr;
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override {
        if (lock)
            InterlockedIncrement(&g_module_refcount);
        else
            InterlockedDecrement(&g_module_refcount);
        return S_OK;
    }

private:
    LONG refcount_;
};

STDAPI DllCanUnloadNow(void) {
    return g_module_refcount == 0 ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID *ppv) {
    HasciiCamVirtualCameraClassFactory *factory;
    HRESULT hr;

    if (ppv == NULL)
        return E_POINTER;
    *ppv = NULL;
    if (!IsEqualCLSID(clsid, *hasciicam_virtual_camera_source_clsid()))
        return CLASS_E_CLASSNOTAVAILABLE;
    factory = new (std::nothrow) HasciiCamVirtualCameraClassFactory();
    if (factory == NULL)
        return E_OUTOFMEMORY;
    hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_module_instance = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

#endif
