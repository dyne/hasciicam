#include "../../include/hasciicam/hasciicam.h"

#include "../app/app_session.h"
#include "../render/render_session.h"
#include "../capture/capture.h"

#include <aalib.h>
#include <stdlib.h>
#include <string.h>

struct hasciicam_instance {
    hasciicam_session session;
    hasciicam_render_session render;
    int started;
    int live_output;
};

static capture_pixel_format map_pixel_format(hasciicam_pixel_format format) {
    switch (format) {
    case HASCIICAM_PIXFMT_GRAY8:
        return CAPTURE_PIXFMT_GRAY8;
    case HASCIICAM_PIXFMT_YUYV:
        return CAPTURE_PIXFMT_YUYV;
    case HASCIICAM_PIXFMT_NV12:
        return CAPTURE_PIXFMT_NV12;
    case HASCIICAM_PIXFMT_NV21:
        return CAPTURE_PIXFMT_NV21;
    case HASCIICAM_PIXFMT_RGB24:
        return CAPTURE_PIXFMT_RGB24;
    case HASCIICAM_PIXFMT_BGR24:
        return CAPTURE_PIXFMT_BGR24;
    case HASCIICAM_PIXFMT_RGB32:
        return CAPTURE_PIXFMT_RGB32;
    case HASCIICAM_PIXFMT_BGRA32:
        return CAPTURE_PIXFMT_BGRA32;
    default:
        return CAPTURE_PIXFMT_UNKNOWN;
    }
}

hasciicam_instance *hasciicam_create(void) {
    hasciicam_instance *instance = (hasciicam_instance *)calloc(1, sizeof(*instance));
    if (instance == NULL)
        return NULL;
    hasciicam_render_session_init(&instance->render);
    return instance;
}

void hasciicam_destroy(hasciicam_instance *instance) {
    if (instance == NULL)
        return;
    hasciicam_stop(instance);
    free(instance);
}

static int hasciicam_start_external_common(hasciicam_instance *instance,
                                           int camera_width,
                                           int camera_height,
                                           int ascii_width,
                                           int ascii_height,
                                           int live_output,
                                           const char *aa_driver) {
    capture_request req;
    if (instance == NULL || camera_width <= 0 || camera_height <= 0 ||
        ascii_width <= 0 || ascii_height <= 0)
        return 0;

    if (instance->started)
        hasciicam_stop(instance);

    memset(&req, 0, sizeof(req));
    req.device = "external://";
    req.requested_width = camera_width;
    req.requested_height = camera_height;
    if (!hasciicam_session_start(&instance->session, &req))
        return 0;

    hasciicam_render_session_configure_geometry(&instance->render, ascii_width, ascii_height);
    instance->render.hwparams.width = ascii_width;
    instance->render.hwparams.height = ascii_height;
    if (live_output) {
        if (aa_driver == NULL || aa_driver[0] == '\0' ||
            !hasciicam_render_session_open_exact_driver(&instance->render, aa_driver)) {
            hasciicam_session_stop(&instance->session);
            return 0;
        }
    } else {
        instance->render.context = aa_init(&mem_d, &instance->render.hwparams, NULL);
    }
    if (instance->render.context == NULL) {
        hasciicam_session_stop(&instance->session);
        return 0;
    }

    instance->started = 1;
    instance->live_output = live_output;
    return 1;
}

int hasciicam_start_external(hasciicam_instance *instance,
                             int camera_width,
                             int camera_height,
                             int ascii_width,
                             int ascii_height) {
    return hasciicam_start_external_common(instance, camera_width, camera_height,
                                           ascii_width, ascii_height, 0, NULL);
}

int hasciicam_start_external_live(hasciicam_instance *instance,
                                  int camera_width,
                                  int camera_height,
                                  int ascii_width,
                                  int ascii_height,
                                  const char *aa_driver) {
    return hasciicam_start_external_common(instance, camera_width, camera_height,
                                           ascii_width, ascii_height, 1, aa_driver);
}

void hasciicam_stop(hasciicam_instance *instance) {
    if (instance == NULL || !instance->started)
        return;
    hasciicam_session_stop(&instance->session);
    hasciicam_render_session_close(&instance->render);
    instance->started = 0;
    instance->live_output = 0;
}

int hasciicam_submit_frame(hasciicam_instance *instance,
                           const unsigned char *data,
                           size_t data_size,
                           int width,
                           int height,
                           int stride_bytes,
                           hasciicam_pixel_format pixel_format) {
    if (instance == NULL || !instance->started)
        return 0;
    return hasciicam_session_submit_frame(data, data_size, width, height,
                                          stride_bytes, map_pixel_format(pixel_format));
}

int hasciicam_render_frame(hasciicam_instance *instance) {
    const unsigned char *gray_frame = NULL;
    int gray_size = 0;
    int dest_size;
    int copy_size;
    if (instance == NULL || !instance->started || instance->render.context == NULL)
        return 0;

    if (!hasciicam_session_step(&instance->session, aa_imgwidth(instance->render.context),
                                aa_imgheight(instance->render.context), &gray_frame,
                                &gray_size)) {
        return 0;
    }

    dest_size = aa_imgwidth(instance->render.context) * aa_imgheight(instance->render.context);
    copy_size = (gray_size < dest_size) ? gray_size : dest_size;
    if (copy_size > 0)
        memcpy(aa_image(instance->render.context), gray_frame, (size_t)copy_size);
    aa_fastrender(instance->render.context, 0, 0,
                  aa_imgwidth(instance->render.context) / 2,
                  aa_imgheight(instance->render.context) / 2);
    if (instance->live_output && !aa_flush_checked(instance->render.context))
        return 0;
    return 1;
}

int hasciicam_get_ascii_frame(const hasciicam_instance *instance,
                              const char **text,
                              const char **attrs,
                              int *width,
                              int *height) {
    hasciicam_ascii_frame frame;
    if (instance == NULL || !instance->started)
        return 0;
    if (!hasciicam_render_session_get_ascii_frame(&instance->render, &frame))
        return 0;
    if (text != NULL)
        *text = frame.text;
    if (attrs != NULL)
        *attrs = frame.attrs;
    if (width != NULL)
        *width = frame.width;
    if (height != NULL)
        *height = frame.height;
    return 1;
}
