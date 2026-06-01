#include "capture_dshow.h"
#include "capture_size.h"

#if defined(_WIN32)

#define COBJMACROS
#include <windows.h>
#include <dshow.h>
#include <dvdmedia.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

/* qedit.h is not available in modern SDKs; define minimal Sample Grabber APIs. */
DEFINE_GUID(CLSID_SampleGrabber, 0xc1f400a0, 0x3f08, 0x11d3, 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37);
DEFINE_GUID(CLSID_NullRenderer, 0xc1f400a4, 0x3f08, 0x11d3, 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37);
DEFINE_GUID(IID_ISampleGrabber, 0x6b652fff, 0x11fe, 0x4fce, 0x92, 0xad, 0x02, 0x66, 0xb5, 0xd7, 0xc7, 0x8f);
DEFINE_GUID(IID_ISampleGrabberCB, 0x0579154a, 0x2b53, 0x4994, 0xb0, 0xd0, 0xe7, 0x73, 0x14, 0x8e, 0xff, 0x85);

struct ISampleGrabberCB : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE SampleCB(double, IMediaSample *) = 0;
    virtual HRESULT STDMETHODCALLTYPE BufferCB(double, BYTE *, long) = 0;
};

struct ISampleGrabber : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE SetOneShot(BOOL) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMediaType(const AM_MEDIA_TYPE *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(AM_MEDIA_TYPE *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(BOOL) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(long *, long *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(IMediaSample **) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCallback(ISampleGrabberCB *, long) = 0;
};

struct capture_device {
    IGraphBuilder *graph;
    ICaptureGraphBuilder2 *builder;
    IBaseFilter *source_filter;
    IBaseFilter *grabber_filter;
    IBaseFilter *null_renderer;
    ISampleGrabber *grabber;
    IMediaControl *media_control;
    IAMVideoProcAmp *video_proc_amp;
    IAMCameraControl *camera_control;
    int com_initialized;
    capture_info info;
    std::vector<unsigned char> buffer;
};

static void dshow_close(capture_device *dev);

static void release_filter(IBaseFilter *f) {
    if (f) f->Release();
}

static void release_media_type(AM_MEDIA_TYPE *mt) {
    if (mt == NULL)
        return;
    if (mt->cbFormat > 0 && mt->pbFormat != NULL) {
        CoTaskMemFree(mt->pbFormat);
        mt->cbFormat = 0;
        mt->pbFormat = NULL;
    }
    if (mt->pUnk != NULL) {
        mt->pUnk->Release();
        mt->pUnk = NULL;
    }
}

static int abs_height(int h) {
    return h > 0 ? h : -h;
}

static int subtype_to_format(const GUID &subtype, capture_pixel_format *fmt) {
    if (IsEqualGUID(subtype, MEDIASUBTYPE_YUY2)) {
        *fmt = CAPTURE_PIXFMT_YUY2;
        return 1;
    }
    if (IsEqualGUID(subtype, MEDIASUBTYPE_RGB24)) {
        *fmt = CAPTURE_PIXFMT_RGB24;
        return 1;
    }
    if (IsEqualGUID(subtype, MEDIASUBTYPE_RGB32)) {
        *fmt = CAPTURE_PIXFMT_RGB32;
        return 1;
    }
    return 0;
}

typedef struct dshow_control_map {
    capture_control_id id;
    int proc_id;
    int cam_id;
    const char *name;
    const char *label;
} dshow_control_map;

static const dshow_control_map g_control_map[] = {
    {CAPTURE_CONTROL_BRIGHTNESS, VideoProcAmp_Brightness, -1, "brightness", "Brightness"},
    {CAPTURE_CONTROL_CONTRAST, VideoProcAmp_Contrast, -1, "contrast", "Contrast"},
    {CAPTURE_CONTROL_GAMMA, VideoProcAmp_Gamma, -1, "gamma", "Gamma"},
    {CAPTURE_CONTROL_GAIN, VideoProcAmp_Gain, -1, "gain", "Gain"},
    {CAPTURE_CONTROL_SATURATION, VideoProcAmp_Saturation, -1, "saturation", "Saturation"},
    {CAPTURE_CONTROL_SHARPNESS, VideoProcAmp_Sharpness, -1, "sharpness", "Sharpness"},
    {CAPTURE_CONTROL_EXPOSURE, -1, CameraControl_Exposure, "exposure", "Exposure"},
    {CAPTURE_CONTROL_WHITE_BALANCE, VideoProcAmp_WhiteBalance, -1, "white_balance", "White Balance"},
    {CAPTURE_CONTROL_FOCUS, -1, CameraControl_Focus, "focus", "Focus"}
};

static const dshow_control_map *find_control_map(capture_control_id id) {
    size_t i;
    for (i = 0; i < sizeof(g_control_map) / sizeof(g_control_map[0]); ++i) {
        if (g_control_map[i].id == id)
            return &g_control_map[i];
    }
    return NULL;
}

static HRESULT configure_capture_size(struct capture_device *dev, const capture_request *req) {
    IAMStreamConfig *stream_config = NULL;
    int best_index = -1;
    int best_w = 0;
    int best_h = 0;
    HRESULT hr;
    int count = 0;
    int cap_size = 0;
    int i;

    if (dev == NULL || req == NULL || req->requested_width <= 0 || req->requested_height <= 0)
        return S_OK;

    hr = dev->builder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
                                     dev->source_filter, IID_IAMStreamConfig,
                                     (void **)&stream_config);
    if (FAILED(hr) || stream_config == NULL)
        return S_OK;

    hr = stream_config->GetNumberOfCapabilities(&count, &cap_size);
    if (FAILED(hr) || count <= 0 || cap_size <= 0) {
        stream_config->Release();
        return S_OK;
    }

    for (i = 0; i < count; i++) {
        AM_MEDIA_TYPE *mt = NULL;
        std::vector<unsigned char> caps((size_t)cap_size);
        VIDEO_STREAM_CONFIG_CAPS *vsc = (VIDEO_STREAM_CONFIG_CAPS *)caps.data();
        capture_pixel_format fmt;
        int w = 0;
        int h = 0;

        hr = stream_config->GetStreamCaps(i, &mt, caps.data());
        if (FAILED(hr) || mt == NULL)
            continue;
        if (mt->formattype != FORMAT_VideoInfo || mt->cbFormat < sizeof(VIDEOINFOHEADER) || mt->pbFormat == NULL) {
            release_media_type(mt);
            CoTaskMemFree(mt);
            continue;
        }
        if (!subtype_to_format(mt->subtype, &fmt)) {
            release_media_type(mt);
            CoTaskMemFree(mt);
            continue;
        }

        w = vsc->InputSize.cx;
        h = abs_height(vsc->InputSize.cy);
        if (w <= 0 || h <= 0) {
            VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)mt->pbFormat;
            w = vih->bmiHeader.biWidth;
            h = abs_height(vih->bmiHeader.biHeight);
        }
        if (w <= 0 || h <= 0) {
            release_media_type(mt);
            CoTaskMemFree(mt);
            continue;
        }

        if (best_index < 0 ||
            capture_size_is_better(req->requested_width, req->requested_height,
                                   best_w, best_h, w, h)) {
            best_index = i;
            best_w = w;
            best_h = h;
        }

        release_media_type(mt);
        CoTaskMemFree(mt);
    }

    if (best_index >= 0) {
        AM_MEDIA_TYPE *best_mt = NULL;
        std::vector<unsigned char> caps((size_t)cap_size);
        hr = stream_config->GetStreamCaps(best_index, &best_mt, caps.data());
        if (SUCCEEDED(hr) && best_mt != NULL) {
            stream_config->SetFormat(best_mt);
            release_media_type(best_mt);
            CoTaskMemFree(best_mt);
        }
    }

    stream_config->Release();
    return S_OK;
}

static int try_match_device(IMoniker *moniker, const capture_request *req) {
    IPropertyBag *bag = NULL;
    VARIANT name;
    wchar_t needle[256];
    int match = 0;
    HRESULT hr;

    if (req->device == NULL || req->device[0] == '\0')
        return 1;

    memset(needle, 0, sizeof(needle));
    MultiByteToWideChar(CP_UTF8, 0, req->device, -1, needle, 255);

    hr = moniker->BindToStorage(NULL, NULL, IID_IPropertyBag, (void **)&bag);
    if (FAILED(hr) || bag == NULL)
        return 0;

    VariantInit(&name);
    hr = bag->Read(L"FriendlyName", &name, NULL);
    if (SUCCEEDED(hr) && name.vt == VT_BSTR && name.bstrVal != NULL) {
        if (wcsstr(name.bstrVal, needle) != NULL)
            match = 1;
    }
    VariantClear(&name);
    bag->Release();
    return match;
}

static HRESULT build_graph(struct capture_device *dev, IMoniker *chosen, const capture_request *req) {
    HRESULT hr;
    AM_MEDIA_TYPE mt;
    VIDEOINFOHEADER *vih;
    long stride;

    hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
                          IID_IGraphBuilder, (void **)&dev->graph);
    if (FAILED(hr)) return hr;

    hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER,
                          IID_ICaptureGraphBuilder2, (void **)&dev->builder);
    if (FAILED(hr)) return hr;
    hr = dev->builder->SetFiltergraph(dev->graph);
    if (FAILED(hr)) return hr;

    hr = chosen->BindToObject(NULL, NULL, IID_IBaseFilter, (void **)&dev->source_filter);
    if (FAILED(hr)) return hr;
    hr = dev->graph->AddFilter(dev->source_filter, L"Video Capture");
    if (FAILED(hr)) return hr;

    hr = configure_capture_size(dev, req);
    if (FAILED(hr)) return hr;

    hr = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,
                          IID_IBaseFilter, (void **)&dev->grabber_filter);
    if (FAILED(hr)) return hr;
    hr = dev->graph->AddFilter(dev->grabber_filter, L"Sample Grabber");
    if (FAILED(hr)) return hr;
    hr = dev->grabber_filter->QueryInterface(IID_ISampleGrabber, (void **)&dev->grabber);
    if (FAILED(hr)) return hr;

    memset(&mt, 0, sizeof(mt));
    mt.majortype = MEDIATYPE_Video;
    mt.subtype = MEDIASUBTYPE_YUY2;
    mt.formattype = FORMAT_VideoInfo;
    hr = dev->grabber->SetMediaType(&mt);
    if (FAILED(hr)) return hr;
    hr = dev->grabber->SetBufferSamples(TRUE);
    if (FAILED(hr)) return hr;
    hr = dev->grabber->SetOneShot(FALSE);
    if (FAILED(hr)) return hr;

    hr = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER,
                          IID_IBaseFilter, (void **)&dev->null_renderer);
    if (FAILED(hr)) return hr;
    hr = dev->graph->AddFilter(dev->null_renderer, L"Null Renderer");
    if (FAILED(hr)) return hr;

    hr = dev->builder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
                                    dev->source_filter, dev->grabber_filter, dev->null_renderer);
    if (FAILED(hr)) return hr;

    memset(&mt, 0, sizeof(mt));
    hr = dev->grabber->GetConnectedMediaType(&mt);
    if (FAILED(hr)) return hr;

    if (mt.formattype != FORMAT_VideoInfo || mt.cbFormat < sizeof(VIDEOINFOHEADER) || mt.pbFormat == NULL) {
        release_media_type(&mt);
        return E_FAIL;
    }

    vih = (VIDEOINFOHEADER *)mt.pbFormat;
    if (!subtype_to_format(mt.subtype, &dev->info.pixel_format)) {
        release_media_type(&mt);
        return E_FAIL;
    }

    dev->info.width = vih->bmiHeader.biWidth;
    dev->info.height = vih->bmiHeader.biHeight > 0 ? vih->bmiHeader.biHeight : -vih->bmiHeader.biHeight;
    stride = vih->bmiHeader.biWidth * (vih->bmiHeader.biBitCount / 8);
    dev->info.stride_bytes = stride;
    release_media_type(&mt);

    hr = dev->graph->QueryInterface(IID_IMediaControl, (void **)&dev->media_control);
    if (SUCCEEDED(hr)) {
        dev->source_filter->QueryInterface(IID_IAMVideoProcAmp, (void **)&dev->video_proc_amp);
        dev->source_filter->QueryInterface(IID_IAMCameraControl, (void **)&dev->camera_control);
    }
    return hr;
}

static int dshow_open(capture_device **out, const capture_request *req) {
    ICreateDevEnum *dev_enum = NULL;
    IEnumMoniker *enum_moniker = NULL;
    IMoniker *moniker = NULL;
    capture_device *dev = NULL;
    HRESULT hr;
    int found = 0;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE) {
    } else {
        fprintf(stderr, "!! directshow COM initialization failed (0x%08lx)\n", (unsigned long)hr);
        return 0;
    }

    dev = new capture_device();
    memset(dev, 0, sizeof(*dev));
    dev->com_initialized = 1;

    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                          IID_ICreateDevEnum, (void **)&dev_enum);
    if (FAILED(hr)) goto fail;

    hr = dev_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enum_moniker, 0);
    if (hr != S_OK || enum_moniker == NULL) goto fail;

    while (enum_moniker->Next(1, &moniker, NULL) == S_OK) {
        if (try_match_device(moniker, req)) {
            hr = build_graph(dev, moniker, req);
            moniker->Release();
            moniker = NULL;
            if (SUCCEEDED(hr)) {
                found = 1;
                break;
            }
        } else {
            moniker->Release();
            moniker = NULL;
        }
    }

    if (!found) goto fail;

    if (enum_moniker) enum_moniker->Release();
    if (dev_enum) dev_enum->Release();
    *out = dev;
    return 1;

fail:
    fprintf(stderr, "!! directshow could not open a compatible capture device\n");
    if (moniker) moniker->Release();
    if (enum_moniker) enum_moniker->Release();
    if (dev_enum) dev_enum->Release();
    dshow_close(dev);
    return 0;
}

static int dshow_describe(capture_device *dev, capture_info *info) {
    if (dev == NULL || info == NULL)
        return 0;
    *info = dev->info;
    return 1;
}

static int dshow_start(capture_device *dev) {
    HRESULT hr;
    if (dev == NULL || dev->media_control == NULL)
        return 0;
    hr = dev->media_control->Run();
    return SUCCEEDED(hr);
}

static int dshow_read(capture_device *dev, capture_frame *frame) {
    long size = 0;
    HRESULT hr;

    if (dev == NULL || dev->grabber == NULL || frame == NULL)
        return 0;

    hr = dev->grabber->GetCurrentBuffer(&size, NULL);
    if (FAILED(hr) || size <= 0)
        return 0;

    dev->buffer.resize((size_t)size);
    hr = dev->grabber->GetCurrentBuffer(&size, (long *)dev->buffer.data());
    if (FAILED(hr) || size <= 0)
        return 0;

    frame->data = dev->buffer.data();
    frame->data_size = (size_t)size;
    frame->width = dev->info.width;
    frame->height = dev->info.height;
    frame->stride_bytes = dev->info.stride_bytes;
    frame->pixel_format = dev->info.pixel_format;
    return 1;
}

static void dshow_release(capture_device *dev, capture_frame *frame) {
    (void)dev;
    if (frame != NULL)
        memset(frame, 0, sizeof(*frame));
}

static void dshow_stop(capture_device *dev) {
    if (dev == NULL || dev->media_control == NULL)
        return;
    dev->media_control->Stop();
}

static void dshow_close(capture_device *dev) {
    if (dev == NULL)
        return;
    dshow_stop(dev);
    if (dev->video_proc_amp) dev->video_proc_amp->Release();
    if (dev->camera_control) dev->camera_control->Release();
    if (dev->media_control) dev->media_control->Release();
    if (dev->grabber) dev->grabber->Release();
    release_filter(dev->null_renderer);
    release_filter(dev->grabber_filter);
    release_filter(dev->source_filter);
    if (dev->builder) dev->builder->Release();
    if (dev->graph) dev->graph->Release();
    if (dev->com_initialized) CoUninitialize();
    delete dev;
}

static int dshow_list_controls(capture_device *dev, capture_control_desc *out, int max_controls) {
    int count = 0;
    size_t i;
    if (dev == NULL || out == NULL || max_controls <= 0)
        return 0;
    for (i = 0; i < sizeof(g_control_map) / sizeof(g_control_map[0]) && count < max_controls; ++i) {
        long minv = 0;
        long maxv = 0;
        long step = 0;
        long defv = 0;
        long caps = 0;
        long cur = 0;
        long flags = 0;
        HRESULT hr = E_FAIL;
        if (g_control_map[i].proc_id >= 0 && dev->video_proc_amp != NULL) {
            hr = dev->video_proc_amp->GetRange(g_control_map[i].proc_id, &minv, &maxv, &step, &defv, &caps);
            if (SUCCEEDED(hr))
                hr = dev->video_proc_amp->Get(g_control_map[i].proc_id, &cur, &flags);
        } else if (g_control_map[i].cam_id >= 0 && dev->camera_control != NULL) {
            hr = dev->camera_control->GetRange(g_control_map[i].cam_id, &minv, &maxv, &step, &defv, &caps);
            if (SUCCEEDED(hr))
                hr = dev->camera_control->Get(g_control_map[i].cam_id, &cur, &flags);
        }
        if (FAILED(hr))
            continue;
        out[count].id = g_control_map[i].id;
        out[count].name = g_control_map[i].name;
        out[count].label = g_control_map[i].label;
        out[count].min_value = (int)minv;
        out[count].max_value = (int)maxv;
        out[count].step = (int)(step > 0 ? step : 1);
        out[count].default_value = (int)defv;
        out[count].current_value = (int)cur;
        out[count].writable = 1;
        out[count].auto_supported = (caps & VideoProcAmp_Flags_Auto) || (caps & CameraControl_Flags_Auto) ? 1 : 0;
        out[count].auto_enabled = ((flags & VideoProcAmp_Flags_Auto) || (flags & CameraControl_Flags_Auto)) ? 1 : 0;
        count++;
    }
    return count;
}

static int dshow_set_control(capture_device *dev, capture_control_id id, int value) {
    const dshow_control_map *map = find_control_map(id);
    if (dev == NULL || map == NULL)
        return 0;
    if (map->proc_id >= 0 && dev->video_proc_amp != NULL)
        return SUCCEEDED(dev->video_proc_amp->Set(map->proc_id, value, VideoProcAmp_Flags_Manual)) ? 1 : 0;
    if (map->cam_id >= 0 && dev->camera_control != NULL)
        return SUCCEEDED(dev->camera_control->Set(map->cam_id, value, CameraControl_Flags_Manual)) ? 1 : 0;
    return 0;
}

static int dshow_set_control_auto(capture_device *dev, capture_control_id id, int enabled) {
    const dshow_control_map *map = find_control_map(id);
    long current = 0;
    long flags = 0;
    if (dev == NULL || map == NULL)
        return 0;
    if (map->proc_id >= 0 && dev->video_proc_amp != NULL) {
        if (FAILED(dev->video_proc_amp->Get(map->proc_id, &current, &flags)))
            return 0;
        return SUCCEEDED(dev->video_proc_amp->Set(map->proc_id, current,
                                                  enabled ? VideoProcAmp_Flags_Auto : VideoProcAmp_Flags_Manual)) ? 1 : 0;
    }
    if (map->cam_id >= 0 && dev->camera_control != NULL) {
        if (FAILED(dev->camera_control->Get(map->cam_id, &current, &flags)))
            return 0;
        return SUCCEEDED(dev->camera_control->Set(map->cam_id, current,
                                                  enabled ? CameraControl_Flags_Auto : CameraControl_Flags_Manual)) ? 1 : 0;
    }
    return 0;
}

#else

struct capture_device {
    int unused;
};

static int dshow_open(capture_device **out, const capture_request *req) {
    (void)out;
    (void)req;
    return 0;
}

static int dshow_describe(capture_device *dev, capture_info *info) {
    (void)dev;
    (void)info;
    return 0;
}

static int dshow_start(capture_device *dev) {
    (void)dev;
    return 0;
}

static int dshow_read(capture_device *dev, capture_frame *frame) {
    (void)dev;
    (void)frame;
    return 0;
}

static void dshow_release(capture_device *dev, capture_frame *frame) {
    (void)dev;
    (void)frame;
}

static void dshow_stop(capture_device *dev) {
    (void)dev;
}

static void dshow_close(capture_device *dev) {
    (void)dev;
}

#endif

static const char *dshow_name(void) {
    return "directshow";
}

static const capture_ops ops = {
    dshow_open,
    dshow_describe,
    dshow_start,
    dshow_read,
    dshow_release,
    dshow_stop,
    dshow_close,
    dshow_name,
    dshow_list_controls,
    dshow_set_control,
    dshow_set_control_auto
};

const capture_ops *capture_dshow_ops(void) {
    return &ops;
}
