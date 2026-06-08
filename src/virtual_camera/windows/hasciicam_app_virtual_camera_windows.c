#ifdef _WIN32

#include "../../app/app_virtual_camera.h"

#include <mfapi.h>
#include <mfvirtualcamera.h>
#include <stdio.h>

static void set_error(char *err, size_t err_size, const char *msg) {
    if (err == NULL || err_size == 0)
        return;
    snprintf(err, err_size, "%s", msg != NULL ? msg : "unknown error");
}

static const char *describe_start_failure_hresult(HRESULT hr) {
    if (hr == E_ACCESSDENIED || hr == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED))
        return "access denied";
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND))
        return "installation missing";
    if (hr == HRESULT_FROM_WIN32(ERROR_BUSY) || hr == E_INVALIDARG)
        return "device busy";
    if (hr == CO_E_ERRORINDLL)
        return "source DLL load failed";
    if (hr == CLASS_E_CLASSNOTAVAILABLE)
        return "registration missing";
    return "registration failed";
}

static void format_hresult_detail(HRESULT hr, char *out, size_t out_size) {
    if (out == NULL || out_size == 0)
        return;
    snprintf(out, out_size, "0x%08lx (%s)", (unsigned long)hr, describe_start_failure_hresult(hr));
}

int hasciicam_app_virtual_camera_windows_start(hasciicam_app_virtual_camera *vc,
                                               const hasciicam_virtual_camera_request *request,
                                               char *err,
                                               size_t err_size) {
    IMFVirtualCamera *virtual_camera = NULL;
    BOOL supported = FALSE;
    HRESULT hr;

    if (vc == NULL || request == NULL) {
        set_error(err, err_size, "virtual camera request is required");
        return 0;
    }
    if (vc->virtual_camera != NULL)
        return 1;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE) {
        vc->windows_com_initialized = 1;
    } else {
        set_error(err, err_size, "virtual camera COM initialization failed");
        return 0;
    }

    hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(hr)) {
        set_error(err, err_size, "virtual camera media foundation startup failed");
        goto fail;
    }
    vc->windows_mf_initialized = 1;

    hr = MFIsVirtualCameraTypeSupported(MFVirtualCameraType_SoftwareCameraSource, &supported);
    if (FAILED(hr) || !supported) {
        set_error(err, err_size, "virtual camera requires Windows 11 build 22000 or later");
        goto fail;
    }

    hr = MFCreateVirtualCamera(MFVirtualCameraType_SoftwareCameraSource,
                               MFVirtualCameraLifetime_Session,
                               MFVirtualCameraAccess_CurrentUser,
                               L"HasciiCam",
                               L"{29E1D0B1-0AF8-4D6F-9D5E-0F9A0F0D4F58}",
                               NULL,
                               0,
                               &virtual_camera);
    if (FAILED(hr) || virtual_camera == NULL) {
        char detail[128];
        snprintf(detail,
                 sizeof(detail),
                 "virtual camera registration failed (%s)",
                 describe_start_failure_hresult(hr));
        set_error(err, err_size, detail);
        goto fail;
    }

    hr = virtual_camera->lpVtbl->Start(virtual_camera, NULL);
    if (FAILED(hr)) {
        char detail[128];
        format_hresult_detail(hr, detail, sizeof(detail));
        {
            char message[192];
            snprintf(message, sizeof(message), "virtual camera start failed (%s)", detail);
            set_error(err, err_size, message);
        }
        goto fail;
    }

    vc->virtual_camera = virtual_camera;
    return 1;

fail:
    if (virtual_camera != NULL) {
        virtual_camera->lpVtbl->Release(virtual_camera);
        virtual_camera = NULL;
    }
    if (vc->windows_mf_initialized) {
        MFShutdown();
        vc->windows_mf_initialized = 0;
    }
    if (vc->windows_com_initialized) {
        CoUninitialize();
        vc->windows_com_initialized = 0;
    }
    return 0;
}

void hasciicam_app_virtual_camera_windows_stop(hasciicam_app_virtual_camera *vc) {
    if (vc == NULL)
        return;
    if (vc->virtual_camera != NULL) {
        vc->virtual_camera->lpVtbl->Stop(vc->virtual_camera);
        vc->virtual_camera->lpVtbl->Shutdown(vc->virtual_camera);
        vc->virtual_camera->lpVtbl->Release(vc->virtual_camera);
        vc->virtual_camera = NULL;
    }
    if (vc->windows_mf_initialized) {
        MFShutdown();
        vc->windows_mf_initialized = 0;
    }
    if (vc->windows_com_initialized) {
        CoUninitialize();
        vc->windows_com_initialized = 0;
    }
}

#endif
