#include "hasciicam_virtual_camera_source.h"

#if defined(_WIN32)

#include <windows.h>
#include <mfapi.h>
#include <stdio.h>
#include <string.h>
#include <unknwn.h>
#include <new>

#include "../pipe/hasciicam_virtual_camera_pipe.h"

static const GUID kHasciiCamVirtualCameraSourceClsid =
{ 0x29e1d0b1, 0x0af8, 0x4d6f, { 0x9d, 0x5e, 0x0f, 0x9a, 0x0f, 0x0d, 0x4f, 0x58 } };

static const wchar_t kHasciiCamVirtualCameraSourceClsidString[] =
    L"{29E1D0B1-0AF8-4D6F-9D5E-0F9A0F0D4F58}";

static LONG g_module_refcount = 0;

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
    },
    {
        HASCIICAM_VIRTUAL_CAMERA_PIXFMT_NV12,
        L"MFVideoFormat_NV12",
        &MFVideoFormat_NV12,
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

unsigned long long hasciicam_virtual_camera_source_sample_duration_100ns(int fps) {
    return hasciicam_virtual_camera_media_type_duration_100ns(fps);
}

unsigned long long hasciicam_virtual_camera_source_sample_time_100ns(unsigned long long start_100ns,
                                                                     unsigned long long sequence,
                                                                     int fps) {
    unsigned long long duration = hasciicam_virtual_camera_source_sample_duration_100ns(fps);
    return start_100ns + sequence * duration;
}

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
        (void)outer;
        (void)riid;
        if (ppv != NULL)
            *ppv = NULL;
        return CLASS_E_CLASSNOTAVAILABLE;
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

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

#endif
