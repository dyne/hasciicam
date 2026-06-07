#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifdef _WIN32
#include <windows.h>
#include <mfapi.h>
#include <mfvirtualcamera.h>
#endif

#include "../src/app/app_virtual_camera.h"

static int failures = 0;
static int g_supported_calls = 0;
static int g_create_calls = 0;
static MFVirtualCameraType g_seen_type = (MFVirtualCameraType)-1;
static MFVirtualCameraLifetime g_seen_lifetime = (MFVirtualCameraLifetime)-1;
static MFVirtualCameraAccess g_seen_access = (MFVirtualCameraAccess)-1;
static wchar_t g_seen_friendly_name[64];
static wchar_t g_seen_source_id[64];

typedef struct fake_virtual_camera {
    IMFVirtualCamera iface;
    LONG refcount;
    int start_calls;
    int stop_calls;
    int shutdown_calls;
    int release_calls;
} fake_virtual_camera;

static fake_virtual_camera g_camera;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        failures++;
    }
}

static HRESULT STDMETHODCALLTYPE fake_virtual_camera_start(IMFVirtualCamera *This, IMFAsyncCallback *pCallback) {
    fake_virtual_camera *camera = (fake_virtual_camera *)This;
    (void)pCallback;
    camera->start_calls++;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE fake_virtual_camera_stop(IMFVirtualCamera *This) {
    fake_virtual_camera *camera = (fake_virtual_camera *)This;
    camera->stop_calls++;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE fake_virtual_camera_shutdown(IMFVirtualCamera *This) {
    fake_virtual_camera *camera = (fake_virtual_camera *)This;
    camera->shutdown_calls++;
    return S_OK;
}

static ULONG STDMETHODCALLTYPE fake_virtual_camera_release(IMFVirtualCamera *This) {
    fake_virtual_camera *camera = (fake_virtual_camera *)This;
    camera->release_calls++;
    if (camera->refcount > 0)
        return (ULONG)InterlockedDecrement(&camera->refcount);
    return 0;
}

static const IMFVirtualCameraVtbl g_fake_virtual_camera_vtbl = {
    .Start = fake_virtual_camera_start,
    .Stop = fake_virtual_camera_stop,
    .Shutdown = fake_virtual_camera_shutdown,
    .Release = fake_virtual_camera_release
};

HRESULT WINAPI MFIsVirtualCameraTypeSupported(MFVirtualCameraType type, BOOL *supported) {
    g_supported_calls++;
    (void)type;
    if (supported != NULL)
        *supported = TRUE;
    return S_OK;
}

HRESULT WINAPI MFCreateVirtualCamera(MFVirtualCameraType type,
                                     MFVirtualCameraLifetime lifetime,
                                     MFVirtualCameraAccess access,
                                     LPCWSTR friendlyName,
                                     LPCWSTR sourceId,
                                     const GUID *categories,
                                     ULONG categoryCount,
                                     IMFVirtualCamera **virtualCamera) {
    if (virtualCamera == NULL)
        return E_POINTER;
    g_create_calls++;
    g_seen_type = type;
    g_seen_lifetime = lifetime;
    g_seen_access = access;
    wcsncpy(g_seen_friendly_name, friendlyName != NULL ? friendlyName : L"", 63);
    g_seen_friendly_name[63] = L'\0';
    wcsncpy(g_seen_source_id, sourceId != NULL ? sourceId : L"", 63);
    g_seen_source_id[63] = L'\0';
    (void)categories;
    (void)categoryCount;
    memset(&g_camera, 0, sizeof(g_camera));
    g_camera.iface.lpVtbl = &g_fake_virtual_camera_vtbl;
    g_camera.refcount = 1;
    *virtualCamera = &g_camera.iface;
    return S_OK;
}

int main(void) {
    hasciicam_app_virtual_camera vc;
    hasciicam_virtual_camera_request request;
    char err[128];

    memset(&vc, 0, sizeof(vc));
    memset(&request, 0, sizeof(request));
    request.enabled = 1;
    request.width = 1280;
    request.height = 720;
    request.fps = 30;

    expect_true(hasciicam_app_virtual_camera_windows_start(&vc, &request, err, sizeof(err)),
                "windows helper should start with a fake MF layer");
    expect_true(g_supported_calls == 1, "virtual camera type support should be queried once");
    expect_true(g_create_calls == 1, "MFCreateVirtualCamera should be called once");
    expect_true(g_seen_type == MFVirtualCameraType_SoftwareCameraSource,
                "virtual camera type should be software camera source");
    expect_true(g_seen_lifetime == MFVirtualCameraLifetime_Session,
                "virtual camera lifetime should be session");
    expect_true(g_seen_access == MFVirtualCameraAccess_CurrentUser,
                "virtual camera access should be current user");
    expect_true(wcscmp(g_seen_friendly_name, L"HasciiCam") == 0,
                "virtual camera friendly name should be HasciiCam");
    expect_true(wcscmp(g_seen_source_id, L"{29E1D0B1-0AF8-4D6F-9D5E-0F9A0F0D4F58}") == 0,
                "virtual camera source id should match the source CLSID");
    expect_true(g_camera.start_calls == 1, "virtual camera object should be started once");
    expect_true(vc.virtual_camera == &g_camera.iface, "controller should retain the started virtual camera");
    expect_true(vc.windows_com_initialized == 1, "COM initialization should be tracked");
    expect_true(vc.windows_mf_initialized == 1, "MF initialization should be tracked");

    hasciicam_app_virtual_camera_windows_stop(&vc);
    expect_true(g_camera.stop_calls == 1, "virtual camera object should be stopped once");
    expect_true(g_camera.shutdown_calls == 1, "virtual camera object should be shutdown once");
    expect_true(g_camera.release_calls == 1, "virtual camera object should be released once");
    expect_true(vc.virtual_camera == NULL, "controller should clear the virtual camera after stop");
    expect_true(vc.windows_com_initialized == 0, "COM initialization flag should be cleared");
    expect_true(vc.windows_mf_initialized == 0, "MF initialization flag should be cleared");

    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
