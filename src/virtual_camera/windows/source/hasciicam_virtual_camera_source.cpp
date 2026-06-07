#include "hasciicam_virtual_camera_source.h"

#if defined(_WIN32)

#include <windows.h>
#include <unknwn.h>
#include <new>

static const GUID kHasciiCamVirtualCameraSourceClsid =
{ 0x29e1d0b1, 0x0af8, 0x4d6f, { 0x9d, 0x5e, 0x0f, 0x9a, 0x0f, 0x0d, 0x4f, 0x58 } };

static const wchar_t kHasciiCamVirtualCameraSourceClsidString[] =
    L"{29E1D0B1-0AF8-4D6F-9D5E-0F9A0F0D4F58}";

static LONG g_module_refcount = 0;

const GUID *hasciicam_virtual_camera_source_clsid(void) {
    return &kHasciiCamVirtualCameraSourceClsid;
}

const wchar_t *hasciicam_virtual_camera_source_clsid_string(void) {
    return kHasciiCamVirtualCameraSourceClsidString;
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
