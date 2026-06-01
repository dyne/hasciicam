#include "capture_mf.h"
#include "capture_size.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define COBJMACROS
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <propvarutil.h>

struct capture_device {
    IMFMediaSource *source;
    IMFSourceReader *reader;
    IMFMediaBuffer *active_buffer;
    IMFSample *active_sample;
    BYTE *active_data;
    DWORD active_length;
    int com_initialized;
    int mf_initialized;
    capture_info info;
};

static wchar_t *to_wide(const char *s) {
    int count;
    wchar_t *out;
    if (s == NULL || *s == '\0')
        return NULL;
    count = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (count <= 0)
        count = MultiByteToWideChar(CP_ACP, 0, s, -1, NULL, 0);
    if (count <= 0)
        return NULL;
    out = (wchar_t *)calloc((size_t)count, sizeof(wchar_t));
    if (out == NULL)
        return NULL;
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, out, count) <= 0 &&
        MultiByteToWideChar(CP_ACP, 0, s, -1, out, count) <= 0) {
        free(out);
        return NULL;
    }
    return out;
}

static capture_pixel_format subtype_to_pixfmt(const GUID *subtype) {
    if (IsEqualGUID(subtype, &MFVideoFormat_YUY2))
        return CAPTURE_PIXFMT_YUY2;
    if (IsEqualGUID(subtype, &MFVideoFormat_NV12))
        return CAPTURE_PIXFMT_NV12;
    if (IsEqualGUID(subtype, &MFVideoFormat_RGB32))
        return CAPTURE_PIXFMT_RGB32;
    if (IsEqualGUID(subtype, &MFVideoFormat_RGB24))
        return CAPTURE_PIXFMT_RGB24;
    return CAPTURE_PIXFMT_UNKNOWN;
}

static int pixfmt_rank(capture_pixel_format fmt) {
    switch (fmt) {
    case CAPTURE_PIXFMT_YUY2: return 0;
    case CAPTURE_PIXFMT_NV12: return 1;
    case CAPTURE_PIXFMT_RGB32: return 2;
    case CAPTURE_PIXFMT_RGB24: return 3;
    default: return 100;
    }
}

static int compute_stride(capture_pixel_format fmt, int width, DWORD *stride) {
    switch (fmt) {
    case CAPTURE_PIXFMT_YUY2:
        *stride = (DWORD)(width * 2);
        return 1;
    case CAPTURE_PIXFMT_NV12:
        *stride = (DWORD)width;
        return 1;
    case CAPTURE_PIXFMT_RGB24:
        *stride = (DWORD)(width * 3);
        return 1;
    case CAPTURE_PIXFMT_RGB32:
        *stride = (DWORD)(width * 4);
        return 1;
    default:
        return 0;
    }
}

static int get_attr_size(IMFAttributes *attrs, const GUID *key, UINT32 *w, UINT32 *h) {
    UINT64 packed = 0;
    HRESULT hr;
    hr = IMFAttributes_GetUINT64(attrs, key, &packed);
    if (FAILED(hr))
        return 0;
    *w = (UINT32)(packed >> 32);
    *h = (UINT32)(packed & 0xffffffffu);
    return 1;
}

static int choose_device(IMFActivate **devices, UINT32 count, const capture_request *req, UINT32 *chosen_index) {
    UINT32 i;
    wchar_t *needle = to_wide(req->device);
    if (needle == NULL || count == 0) {
        *chosen_index = 0;
        free(needle);
        return count > 0;
    }

    for (i = 0; i < count; i++) {
        WCHAR *friendly = NULL;
        WCHAR *symbolic = NULL;
        UINT32 friendly_len = 0;
        UINT32 symbolic_len = 0;
        HRESULT hr1 = IMFActivate_GetAllocatedString(devices[i], &MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
                                                      &friendly, &friendly_len);
        HRESULT hr2 = IMFActivate_GetAllocatedString(devices[i], &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
                                                      &symbolic, &symbolic_len);
        int match = 0;
        (void)friendly_len;
        (void)symbolic_len;

        if (SUCCEEDED(hr1) && friendly != NULL && wcsstr(friendly, needle) != NULL)
            match = 1;
        if (SUCCEEDED(hr2) && symbolic != NULL && _wcsicmp(symbolic, needle) == 0)
            match = 1;

        if (friendly) CoTaskMemFree(friendly);
        if (symbolic) CoTaskMemFree(symbolic);

        if (match) {
            *chosen_index = i;
            free(needle);
            return 1;
        }
    }

    free(needle);
    return 0;
}

static int mf_open(capture_device **out, const capture_request *req) {
    capture_device *dev = NULL;
    IMFAttributes *attrs = NULL;
    IMFActivate **devices = NULL;
    IMFMediaType *chosen = NULL;
    IMFMediaType *current = NULL;
    UINT32 count = 0;
    UINT32 chosen_idx = 0;
    HRESULT hr;
    UINT32 i;
    int best_rank = 1000;
    int best_size_score = 0x7fffffff;
    int found = 0;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE) {
        /* RPC_E_CHANGED_MODE still allows COM usage in-process. */
    } else {
        fprintf(stderr, "!! COM initialization failed (0x%08lx)\n", (unsigned long)hr);
        return 0;
    }

    hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(hr)) {
        fprintf(stderr, "!! media foundation startup failed (0x%08lx)\n", (unsigned long)hr);
        CoUninitialize();
        return 0;
    }

    dev = (capture_device *)calloc(1, sizeof(*dev));
    if (dev == NULL)
        goto fail;
    dev->com_initialized = 1;
    dev->mf_initialized = 1;

    hr = MFCreateAttributes(&attrs, 1);
    if (FAILED(hr))
        goto fail;
    hr = IMFAttributes_SetGUID(attrs, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                               &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr))
        goto fail;
    hr = MFEnumDeviceSources(attrs, &devices, &count);
    if (FAILED(hr) || count == 0) {
        fprintf(stderr, "!! no media foundation video capture device was found\n");
        goto fail;
    }

    if (!choose_device(devices, count, req, &chosen_idx)) {
        fprintf(stderr, "!! requested media foundation device was not found\n");
        goto fail;
    }

    hr = IMFActivate_ActivateObject(devices[chosen_idx], &IID_IMFMediaSource, (void **)&dev->source);
    if (FAILED(hr))
        goto fail;

    hr = MFCreateSourceReaderFromMediaSource(dev->source, NULL, &dev->reader);
    if (FAILED(hr))
        goto fail;

    for (i = 0; ; i++) {
        IMFMediaType *candidate = NULL;
        GUID subtype = GUID_NULL;
        UINT32 w = 0, h = 0;
        capture_pixel_format fmt;
        int rank;

        hr = IMFSourceReader_GetNativeMediaType(dev->reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &candidate);
        if (hr == MF_E_NO_MORE_TYPES)
            break;
        if (FAILED(hr) || candidate == NULL)
            continue;

        if (FAILED(IMFMediaType_GetGUID(candidate, &MF_MT_SUBTYPE, &subtype))) {
            IMFMediaType_Release(candidate);
            continue;
        }
        fmt = subtype_to_pixfmt(&subtype);
        rank = pixfmt_rank(fmt);
        if (rank >= 100) {
            IMFMediaType_Release(candidate);
            continue;
        }
        if (!get_attr_size((IMFAttributes *)candidate, &MF_MT_FRAME_SIZE, &w, &h)) {
            IMFMediaType_Release(candidate);
            continue;
        }

        if (req->requested_width > 0 && req->requested_height > 0) {
            int score = capture_size_score(req->requested_width, req->requested_height, (int)w, (int)h);
            if (!found ||
                score < best_size_score ||
                (score == best_size_score && rank < best_rank)) {
                if (chosen != NULL)
                    IMFMediaType_Release(chosen);
                chosen = candidate;
                best_rank = rank;
                best_size_score = score;
                found = 1;
            } else {
                IMFMediaType_Release(candidate);
            }
            continue;
        }

        if (!found || rank < best_rank) {
            if (chosen != NULL)
                IMFMediaType_Release(chosen);
            chosen = candidate;
            best_rank = rank;
            found = 1;
        } else {
            IMFMediaType_Release(candidate);
        }
    }

    if (!found) {
        fprintf(stderr, "!! no compatible uncompressed media foundation format was found\n");
        goto fail;
    }

    hr = IMFSourceReader_SetCurrentMediaType(dev->reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, chosen);
    if (FAILED(hr))
        goto fail;

    hr = IMFSourceReader_GetCurrentMediaType(dev->reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, &current);
    if (FAILED(hr) || current == NULL)
        goto fail;

    {
        GUID subtype = GUID_NULL;
        UINT32 w = 0, h = 0;
        UINT32 stride = 0;
        capture_pixel_format fmt;
        if (FAILED(IMFMediaType_GetGUID(current, &MF_MT_SUBTYPE, &subtype)))
            goto fail;
        fmt = subtype_to_pixfmt(&subtype);
        if (fmt == CAPTURE_PIXFMT_UNKNOWN)
            goto fail;
        if (!get_attr_size((IMFAttributes *)current, &MF_MT_FRAME_SIZE, &w, &h))
            goto fail;
        if (FAILED(IMFMediaType_GetUINT32(current, &MF_MT_DEFAULT_STRIDE, &stride))) {
            if (!compute_stride(fmt, (int)w, (DWORD *)&stride))
                goto fail;
        }
        dev->info.width = (int)w;
        dev->info.height = (int)h;
        dev->info.stride_bytes = (int)stride;
        dev->info.pixel_format = fmt;
    }

    if (current) IMFMediaType_Release(current);
    if (chosen) IMFMediaType_Release(chosen);
    if (devices) {
        for (i = 0; i < count; i++) IMFActivate_Release(devices[i]);
        CoTaskMemFree(devices);
    }
    if (attrs) IMFAttributes_Release(attrs);

    *out = dev;
    return 1;

fail:
    if (current) IMFMediaType_Release(current);
    if (chosen) IMFMediaType_Release(chosen);
    if (devices) {
        for (i = 0; i < count; i++) IMFActivate_Release(devices[i]);
        CoTaskMemFree(devices);
    }
    if (attrs) IMFAttributes_Release(attrs);
    if (dev) {
        if (dev->reader) IMFSourceReader_Release(dev->reader);
        if (dev->source) IMFMediaSource_Release(dev->source);
        if (dev->mf_initialized) MFShutdown();
        if (dev->com_initialized) CoUninitialize();
        free(dev);
    } else {
        MFShutdown();
        CoUninitialize();
    }
    return 0;
}

static int mf_describe(capture_device *dev, capture_info *info) {
    if (dev == NULL || info == NULL)
        return 0;
    *info = dev->info;
    return 1;
}

static int mf_start(capture_device *dev) {
    (void)dev;
    return 1;
}

static int mf_read(capture_device *dev, capture_frame *frame) {
    HRESULT hr;
    int attempt;

    if (dev == NULL || frame == NULL || dev->reader == NULL)
        return 0;

    for (attempt = 0; attempt < 30; attempt++) {
        IMFSample *sample = NULL;
        IMFMediaBuffer *buffer = NULL;
        DWORD stream_flags = 0;
        LONGLONG timestamp = 0;
        DWORD length = 0;
        BYTE *data = NULL;

        hr = IMFSourceReader_ReadSample(dev->reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
                                        NULL, &stream_flags, &timestamp, &sample);
        (void)timestamp;
        if (FAILED(hr)) {
            Sleep(10);
            continue;
        }
        if (stream_flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            if (sample) IMFSample_Release(sample);
            return 0;
        }
        if (stream_flags & MF_SOURCE_READERF_STREAMTICK) {
            if (sample) IMFSample_Release(sample);
            Sleep(10);
            continue;
        }
        if (sample == NULL) {
            Sleep(10);
            continue;
        }

        hr = IMFSample_ConvertToContiguousBuffer(sample, &buffer);
        if (FAILED(hr) || buffer == NULL) {
            IMFSample_Release(sample);
            Sleep(10);
            continue;
        }

        hr = IMFMediaBuffer_Lock(buffer, &data, NULL, &length);
        if (FAILED(hr) || data == NULL || length == 0) {
            IMFMediaBuffer_Release(buffer);
            IMFSample_Release(sample);
            Sleep(10);
            continue;
        }

        dev->active_buffer = buffer;
        dev->active_sample = sample;
        dev->active_data = data;
        dev->active_length = length;

        frame->data = data;
        frame->data_size = (size_t)length;
        frame->width = dev->info.width;
        frame->height = dev->info.height;
        frame->stride_bytes = dev->info.stride_bytes;
        frame->pixel_format = dev->info.pixel_format;
        return 1;
    }

    return 0;
}

static void mf_release(capture_device *dev, capture_frame *frame) {
    if (dev == NULL)
        return;
    if (dev->active_buffer != NULL) {
        IMFMediaBuffer_Unlock(dev->active_buffer);
        IMFMediaBuffer_Release(dev->active_buffer);
        dev->active_buffer = NULL;
    }
    if (dev->active_sample != NULL) {
        IMFSample_Release(dev->active_sample);
        dev->active_sample = NULL;
    }
    dev->active_data = NULL;
    dev->active_length = 0;
    if (frame != NULL)
        memset(frame, 0, sizeof(*frame));
}

static void mf_stop(capture_device *dev) {
    if (dev == NULL)
        return;
    mf_release(dev, NULL);
}

static void mf_close(capture_device *dev) {
    if (dev == NULL)
        return;
    mf_stop(dev);
    if (dev->reader) IMFSourceReader_Release(dev->reader);
    if (dev->source) IMFMediaSource_Release(dev->source);
    if (dev->mf_initialized) MFShutdown();
    if (dev->com_initialized) CoUninitialize();
    free(dev);
}

#else

struct capture_device {
    int unused;
};

static int mf_open(capture_device **out, const capture_request *req) {
    (void)out;
    (void)req;
    fprintf(stderr, "!! media foundation backend is only available on Windows\n");
    return 0;
}

static int mf_describe(capture_device *dev, capture_info *info) {
    (void)dev;
    (void)info;
    return 0;
}

static int mf_start(capture_device *dev) {
    (void)dev;
    return 0;
}

static int mf_read(capture_device *dev, capture_frame *frame) {
    (void)dev;
    (void)frame;
    return 0;
}

static void mf_release(capture_device *dev, capture_frame *frame) {
    (void)dev;
    (void)frame;
}

static void mf_stop(capture_device *dev) {
    (void)dev;
}

static void mf_close(capture_device *dev) {
    (void)dev;
}

#endif

static const char *mf_name(void) {
    return "media-foundation";
}

static const capture_ops ops = {
    mf_open,
    mf_describe,
    mf_start,
    mf_read,
    mf_release,
    mf_stop,
    mf_close,
    mf_name,
    NULL,
    NULL,
    NULL
};

const capture_ops *capture_mf_ops(void) {
    return &ops;
}
