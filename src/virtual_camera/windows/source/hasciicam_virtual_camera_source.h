#ifndef HASCIICAM_VIRTUAL_CAMERA_SOURCE_H
#define HASCIICAM_VIRTUAL_CAMERA_SOURCE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <wchar.h>
#include <guiddef.h>

/**
 * Return the CLSID used to register the HasciiCam virtual camera source.
 */
const GUID *hasciicam_virtual_camera_source_clsid(void);

/**
 * Return the CLSID in canonical "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}" form.
 */
const wchar_t *hasciicam_virtual_camera_source_clsid_string(void);

#ifdef __cplusplus
}
#endif

#endif
