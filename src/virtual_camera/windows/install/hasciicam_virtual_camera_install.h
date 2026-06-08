#ifndef HASCIICAM_VIRTUAL_CAMERA_INSTALL_H
#define HASCIICAM_VIRTUAL_CAMERA_INSTALL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#ifdef _WIN32
#include <wchar.h>
#endif

/**
 * Return the CLSID used by the Windows virtual camera source.
 */
const wchar_t *hasciicam_virtual_camera_install_clsid_string(void);

/**
 * Build the default install root beneath Program Files.
 */
int hasciicam_virtual_camera_install_default_root(wchar_t *out,
                                                  size_t out_size,
                                                  char *err,
                                                  size_t err_size);

/**
 * Build the default DLL install path beneath the install root.
 */
int hasciicam_virtual_camera_install_default_dll_path(wchar_t *out,
                                                      size_t out_size,
                                                      char *err,
                                                      size_t err_size);

/**
 * Build the registry key for the COM class registration.
 */
int hasciicam_virtual_camera_install_registry_key(wchar_t *out,
                                                  size_t out_size,
                                                  char *err,
                                                  size_t err_size);

/**
 * Return whether this build creates the Windows source trace log.
 */
int hasciicam_virtual_camera_install_debug_logging_enabled(void);

/**
 * Copy the source DLL into the install root.
 */
int hasciicam_virtual_camera_install_copy_dll(const wchar_t *source_path,
                                              const wchar_t *dest_path,
                                              char *err,
                                              size_t err_size);

/**
 * Remove the installed source DLL after a short wait for Windows Frame Server.
 */
int hasciicam_virtual_camera_install_remove_dll(const wchar_t *dll_path,
                                                char *err,
                                                size_t err_size);

/**
 * Register the DLL for machine-wide COM activation.
 */
int hasciicam_virtual_camera_install_register(const wchar_t *dll_path,
                                              char *err,
                                              size_t err_size);

/**
 * Remove the machine-wide COM registration.
 */
int hasciicam_virtual_camera_install_unregister(char *err,
                                                size_t err_size);

/**
 * Return whether the DLL exists and the registration key is present.
 */
int hasciicam_virtual_camera_install_status(const wchar_t *dll_path,
                                            int *dll_exists,
                                            int *registered,
                                            char *err,
                                            size_t err_size);

#ifdef __cplusplus
}
#endif

#endif
