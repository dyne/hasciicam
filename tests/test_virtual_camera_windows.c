#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef _WIN32
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#endif

#include "../src/virtual_camera/windows/source/hasciicam_virtual_camera_source.h"
#include "../src/virtual_camera/windows/pipe/hasciicam_virtual_camera_pipe.h"

#ifdef _WIN32
STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID *ppv);
STDAPI DllCanUnloadNow(void);

typedef struct pipe_writer_context {
    HANDLE pipe_handle;
    const unsigned char *message;
    DWORD message_size;
} pipe_writer_context;

typedef struct backend_reader_context {
    hasciicam_virtual_camera_source_config config;
    hasciicam_virtual_camera_source_frame_slot slot;
    char err[128];
    int ok;
} backend_reader_context;

static DWORD WINAPI pipe_writer_thread(LPVOID param) {
    pipe_writer_context *ctx = (pipe_writer_context *)param;
    DWORD bytes_written = 0;

    if (ctx == NULL || ctx->pipe_handle == INVALID_HANDLE_VALUE)
        return 0;
    if (ConnectNamedPipe(ctx->pipe_handle, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
        (void)WriteFile(ctx->pipe_handle, ctx->message, ctx->message_size, &bytes_written, NULL);
        FlushFileBuffers(ctx->pipe_handle);
        DisconnectNamedPipe(ctx->pipe_handle);
    }
    return bytes_written == ctx->message_size ? 1 : 0;
}

static DWORD WINAPI backend_reader_thread(LPVOID param) {
    backend_reader_context *ctx = (backend_reader_context *)param;

    if (ctx == NULL)
        return 0;
    hasciicam_virtual_camera_source_frame_slot_init(&ctx->slot);
    ctx->ok = hasciicam_virtual_camera_source_read_pipe_message(&ctx->config,
                                                                ctx->config.pipe_name,
                                                                &ctx->slot,
                                                                2000,
                                                                ctx->err,
                                                                sizeof(ctx->err));
    return ctx->ok ? 1 : 0;
}
#endif

static int failures = 0;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        failures++;
    }
}

#ifdef _WIN32
static void run_windows_com_smoke(void) {
    IClassFactory *factory = NULL;
    IMFActivate *activate = NULL;
    IMFMediaSourceEx *source = NULL;
    IMFPresentationDescriptor *presentation = NULL;
    IMFAttributes *source_attributes = NULL;
    IMFAttributes *stream_attributes = NULL;
    IMFGetService *get_service = NULL;
    DWORD characteristics = 0;
    HRESULT hr;
    BOOL com_initialized = FALSE;
    BOOL mf_started = FALSE;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    expect_true(SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE,
                "COM apartment should initialize for the Windows smoke test");
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
        return;
    com_initialized = (hr == S_OK || hr == S_FALSE);

    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    expect_true(SUCCEEDED(hr), "Media Foundation should start for the Windows smoke test");
    if (FAILED(hr)) {
        if (com_initialized)
            CoUninitialize();
        return;
    }
    mf_started = TRUE;

    hr = DllGetClassObject(hasciicam_virtual_camera_source_clsid(), &IID_IClassFactory, (void **)&factory);
    expect_true(SUCCEEDED(hr) && factory != NULL, "source class factory should be available");
    if (SUCCEEDED(hr) && factory != NULL) {
        hr = factory->lpVtbl->CreateInstance(factory, NULL, &IID_IMFActivate, (void **)&activate);
        expect_true(SUCCEEDED(hr) && activate != NULL, "source activation object should be created");
    }
    if (activate != NULL) {
        hr = activate->lpVtbl->ActivateObject(activate, &IID_IMFMediaSourceEx, (void **)&source);
        expect_true(SUCCEEDED(hr) && source != NULL, "source object should be activated");
    }

    if (source != NULL) {
        hr = source->lpVtbl->GetCharacteristics(source, &characteristics);
        expect_true(SUCCEEDED(hr), "source characteristics should be available");
        if (SUCCEEDED(hr)) {
            expect_true((characteristics & MFMEDIASOURCE_IS_LIVE) != 0,
                        "source should report itself as live");
            expect_true((characteristics & MFMEDIASOURCE_DOES_NOT_USE_NETWORK) != 0,
                        "source should not use the network");
        }

        hr = source->lpVtbl->CreatePresentationDescriptor(source, &presentation);
        expect_true(SUCCEEDED(hr) && presentation != NULL,
                    "source should create a presentation descriptor");

        hr = source->lpVtbl->GetSourceAttributes(source, &source_attributes);
        expect_true(SUCCEEDED(hr) && source_attributes != NULL,
                    "source attributes should be available");

        hr = source->lpVtbl->GetStreamAttributes(source, 0, &stream_attributes);
        expect_true(SUCCEEDED(hr) && stream_attributes != NULL,
                    "stream attributes should be available");

        hr = source->lpVtbl->QueryInterface(source, &IID_IMFGetService, (void **)&get_service);
        expect_true(SUCCEEDED(hr) && get_service != NULL,
                    "source should expose IMFGetService");
        if (get_service != NULL)
            get_service->lpVtbl->Release(get_service);

        hr = source->lpVtbl->Start(source, presentation, NULL, NULL);
        expect_true(SUCCEEDED(hr), "source should start");
        if (SUCCEEDED(hr)) {
            expect_true(FAILED(source->lpVtbl->Start(source, presentation, NULL, NULL)),
                        "starting twice should be rejected");
            expect_true(SUCCEEDED(source->lpVtbl->Stop(source)),
                        "source should stop");
        }

        expect_true(SUCCEEDED(source->lpVtbl->Shutdown(source)),
                    "source should shut down");
    }

    if (stream_attributes != NULL)
        stream_attributes->lpVtbl->Release(stream_attributes);
    if (source_attributes != NULL)
        source_attributes->lpVtbl->Release(source_attributes);
    if (presentation != NULL)
        presentation->lpVtbl->Release(presentation);
    if (source != NULL)
        source->lpVtbl->Release(source);
    if (activate != NULL) {
        activate->lpVtbl->ShutdownObject(activate);
        activate->lpVtbl->DetachObject(activate);
        activate->lpVtbl->Release(activate);
    }
    if (factory != NULL)
        factory->lpVtbl->Release(factory);

    expect_true(DllCanUnloadNow() == S_OK,
                "source DLL should be unloadable after COM cleanup");

    if (mf_started)
        MFShutdown();
    if (com_initialized)
        CoUninitialize();
}

static void run_windows_dll_export_smoke(void) {
    HMODULE module;

    module = LoadLibraryW(L"hasciicam_virtual_camera_source.dll");
    expect_true(module != NULL, "source DLL should load from the test binary directory");
    if (module == NULL)
        return;
    expect_true(GetProcAddress(module, "DllGetClassObject") != NULL,
                "source DLL should export DllGetClassObject");
    expect_true(GetProcAddress(module, "DllCanUnloadNow") != NULL,
                "source DLL should export DllCanUnloadNow");
    FreeLibrary(module);
}

static void run_windows_pipe_roundtrip(void) {
    hasciicam_virtual_camera_request request;
    hasciicam_virtual_camera_source_config config;
    hasciicam_virtual_camera_source_frame_slot slot;
    hasciicam_virtual_camera_pipe_frame frame;
    unsigned char *message = NULL;
    HANDLE pipe_handle = INVALID_HANDLE_VALUE;
    HANDLE thread_handle = NULL;
    pipe_writer_context writer;
    DWORD thread_exit = 0;
    char err[128];
    size_t message_size;
    const unsigned char *payload = NULL;
    size_t payload_size = 0;

    hasciicam_virtual_camera_request_init(&request);
    request.enabled = 1;
    request.width = 1280;
    request.height = 720;
    request.fps = 30;
    request.device[0] = '\0';
    expect_true(hasciicam_virtual_camera_source_config_prepare(&request, &config, err, sizeof(err)),
                "pipe roundtrip should prepare source config");
    hasciicam_virtual_camera_source_frame_slot_init(&slot);

    hasciicam_virtual_camera_pipe_frame_init(&frame,
                                             HASCIICAM_VIRTUAL_CAMERA_PIXFMT_YUY2,
                                             1280,
                                             720,
                                             2560,
                                             21ULL,
                                             555555555ULL);
    message_size = sizeof(frame) + 1843200U;
    message = (unsigned char *)malloc(message_size);
    expect_true(message != NULL, "pipe roundtrip message should allocate");
    if (message == NULL)
        return;
    memcpy(message, &frame, sizeof(frame));
    memset(message + sizeof(frame), 0x44, 1843200U);

    pipe_handle = CreateNamedPipeA(config.pipe_name,
                                   PIPE_ACCESS_OUTBOUND,
                                   PIPE_TYPE_BYTE | PIPE_WAIT,
                                   1,
                                   (DWORD)message_size,
                                   (DWORD)message_size,
                                   0,
                                   NULL);
    expect_true(pipe_handle != INVALID_HANDLE_VALUE, "pipe server should create a named pipe");
    if (pipe_handle == INVALID_HANDLE_VALUE) {
        free(message);
        return;
    }

    writer.pipe_handle = pipe_handle;
    writer.message = message;
    writer.message_size = (DWORD)message_size;
    thread_handle = CreateThread(NULL, 0, pipe_writer_thread, &writer, 0, NULL);
    expect_true(thread_handle != NULL, "pipe writer thread should start");
    if (thread_handle != NULL) {
        int read_ok = hasciicam_virtual_camera_source_read_pipe_message(&config,
                                                                        config.pipe_name,
                                                                        &slot,
                                                                        1000,
                                                                        err,
                                                                        sizeof(err));
        expect_true(read_ok, "pipe reader should decode one complete message");
        if (!read_ok) {
            fprintf(stderr, "pipe helper error: %s\n", err);
        }
        expect_true(hasciicam_virtual_camera_source_frame_slot_has_message(&slot),
                    "pipe reader should store the newest message");
        expect_true(slot.sequence == 21ULL, "pipe reader should preserve the frame sequence");
        expect_true(slot.timestamp_100ns == 555555555ULL,
                    "pipe reader should preserve the frame timestamp");
        expect_true(slot.bytes_size == message_size,
                    "pipe reader should retain the exact message size");
        expect_true(slot.bytes != NULL && memcmp(slot.bytes, message, message_size) == 0,
                    "pipe reader should copy the exact message bytes");
        WaitForSingleObject(thread_handle, INFINITE);
        GetExitCodeThread(thread_handle, &thread_exit);
        expect_true(thread_exit == 1, "pipe writer should send the full message");
        CloseHandle(thread_handle);
    }

    hasciicam_virtual_camera_source_frame_slot_close(&slot);
    free(message);
    if (pipe_handle != INVALID_HANDLE_VALUE)
        CloseHandle(pipe_handle);
}

static void run_windows_backend_roundtrip(void) {
    hasciicam_virtual_camera_request request;
    hasciicam_virtual_camera_device *device = NULL;
    hasciicam_virtual_camera_source_config config;
    backend_reader_context reader;
    HANDLE reader_thread = NULL;
    hasciicam_virtual_camera_frame frame;
    unsigned char pixels[16] = {
        0, 0, 0, 255,
        0, 0, 0, 255,
        0, 0, 0, 255,
        0, 0, 0, 255
    };
    char err[128];
    IMFSample *sample = NULL;
    IMFMediaBuffer *sample_buffer = NULL;
    BYTE *sample_data = NULL;
    DWORD sample_max = 0;
    DWORD sample_len = 0;
    LONGLONG sample_time = 0;
    LONGLONG sample_duration = 0;

    hasciicam_virtual_camera_request_init(&request);
    request.enabled = 1;
    request.width = 2;
    request.height = 2;
    request.fps = 30;
    expect_true(hasciicam_virtual_camera_open_default(&device, &request, err, sizeof(err)),
                "windows backend should open");
    expect_true(device != NULL, "windows backend should allocate");
    expect_true(hasciicam_virtual_camera_is_supported(device) == 1,
                "windows backend should report supported");
    expect_true(strcmp(hasciicam_virtual_camera_backend_name(device), "windows-pipe") == 0,
                "windows backend should name itself");

    expect_true(hasciicam_virtual_camera_source_config_prepare(&request, &config, err, sizeof(err)),
                "backend roundtrip should prepare source config");
    memset(&reader, 0, sizeof(reader));
    reader.config = config;
    reader_thread = CreateThread(NULL, 0, backend_reader_thread, &reader, 0, NULL);
    expect_true(reader_thread != NULL, "backend reader thread should start");
    if (reader_thread != NULL) {
        Sleep(50);
        memset(&frame, 0, sizeof(frame));
        frame.pixels = pixels;
        frame.width = 2;
        frame.height = 2;
        frame.stride_bytes = 8;
        frame.pixel_format = HASCIICAM_VIRTUAL_CAMERA_PIXFMT_BGRA32;
        frame.timestamp_100ns = 777777777ULL;
        expect_true(hasciicam_virtual_camera_publish(device, &frame),
                    "windows backend should publish a frame");
        WaitForSingleObject(reader_thread, INFINITE);
        expect_true(reader.ok, "backend reader should decode the published message");
        if (reader.ok) {
            expect_true(hasciicam_virtual_camera_source_frame_slot_has_message(&reader.slot),
                        "backend reader should retain a message");
            expect_true(reader.slot.timestamp_100ns == 777777777ULL,
                        "backend reader should preserve the timestamp");
            expect_true(reader.slot.sequence == 0ULL,
                        "backend reader should publish the first sequence");
            expect_true(reader.slot.bytes_size == sizeof(hasciicam_virtual_camera_pipe_frame) + 8U,
                        "backend reader should store the exact message size");
            expect_true(reader.slot.bytes != NULL &&
                        reader.slot.bytes[sizeof(hasciicam_virtual_camera_pipe_frame) + 0] == 0x10 &&
                        reader.slot.bytes[sizeof(hasciicam_virtual_camera_pipe_frame) + 1] == 0x80 &&
                        reader.slot.bytes[sizeof(hasciicam_virtual_camera_pipe_frame) + 2] == 0x10 &&
                        reader.slot.bytes[sizeof(hasciicam_virtual_camera_pipe_frame) + 3] == 0x80,
                        "backend reader should receive a black YUY2 frame");
        } else {
            fprintf(stderr, "backend helper error: %s\n", reader.err);
        }
        expect_true(hasciicam_virtual_camera_source_make_sample(&reader.config,
                                                                &reader.slot,
                                                                888888888ULL,
                                                                3ULL,
                                                                &sample,
                                                                err,
                                                                sizeof(err)),
                    "backend roundtrip should build a timed sample");
        if (sample != NULL) {
            expect_true(SUCCEEDED(sample->lpVtbl->GetSampleTime(sample, &sample_time)),
                        "sample should expose a timestamp");
            expect_true(SUCCEEDED(sample->lpVtbl->GetSampleDuration(sample, &sample_duration)),
                        "sample should expose a duration");
            expect_true(SUCCEEDED(sample->lpVtbl->ConvertToContiguousBuffer(sample, &sample_buffer)),
                        "sample should expose a contiguous buffer");
            if (sample_buffer != NULL) {
                expect_true(SUCCEEDED(sample_buffer->lpVtbl->Lock(sample_buffer, &sample_data, &sample_max, &sample_len)),
                            "sample buffer should lock");
                if (sample_data != NULL) {
                    expect_true(sample_time == 777777777ULL,
                                "sample should preserve the frame timestamp");
                    expect_true(sample_duration == 333333ULL,
                                "sample should use the configured duration");
                    expect_true(sample_len == 8U, "sample payload should be one YUY2 frame");
                    expect_true(sample_data[0] == 0x10 && sample_data[1] == 0x80 &&
                                sample_data[2] == 0x10 && sample_data[3] == 0x80,
                                "sample payload should remain black");
                }
                sample_buffer->lpVtbl->Unlock(sample_buffer);
                sample_buffer->lpVtbl->Release(sample_buffer);
            }
            sample->lpVtbl->Release(sample);
            sample = NULL;
            sample_buffer = NULL;
        }
        hasciicam_virtual_camera_source_frame_slot_close(&reader.slot);
        CloseHandle(reader_thread);
    }

    hasciicam_virtual_camera_close(device);
}
#endif

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
            expect_true(strstr(pipe_name, "\\\\.\\pipe\\HasciiCam_") == pipe_name,
                        "pipe name should use the expected prefix");
            expect_true(strstr(pipe_name, "_Device_One_Two_") != NULL,
                        "pipe name should sanitize device characters");
            expect_true(strstr(pipe_name, "_1280x720@30") != NULL,
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
            expect_true(strstr(payload, "pipe=\\\\.\\pipe\\HasciiCam_") != NULL,
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
                                                         message_size,
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

#ifdef _WIN32
    run_windows_dll_export_smoke();
    run_windows_com_smoke();
    run_windows_pipe_roundtrip();
    run_windows_backend_roundtrip();
#endif

    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
