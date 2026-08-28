#include <hasciicam/hasciicam.h>

#include <stddef.h>
#include <stdlib.h>

static hasciicam_instance *g_instance = NULL;
static const char *g_last_text = NULL;
static int g_last_width = 0;
static int g_last_height = 0;

int hasciicam_wasm_init(int camera_width, int camera_height, int ascii_width, int ascii_height,
                        int software_renderer) {
    if (g_instance != NULL) {
        hasciicam_destroy(g_instance);
        g_instance = NULL;
    }
    g_instance = hasciicam_create();
    if (g_instance == NULL) {
        return 0;
    }
    if (software_renderer) {
        setenv("HASCIICAM_SDL_RENDERER", "software", 1);
        setenv("HASCIICAM_SDL_VSYNC", "off", 1);
    } else {
        setenv("HASCIICAM_SDL_RENDERER", "accelerated", 1);
        setenv("HASCIICAM_SDL_VSYNC", "on", 1);
    }
    return hasciicam_start_external_live(g_instance, camera_width, camera_height,
                                         ascii_width, ascii_height, "SDL");
}

int hasciicam_wasm_submit_rgba(const unsigned char *data, size_t data_size, int width, int height, int stride) {
    if (g_instance == NULL) {
        return 0;
    }
    return hasciicam_submit_frame(g_instance, data, data_size, width, height, stride, HASCIICAM_PIXFMT_RGB32);
}

int hasciicam_wasm_render(void) {
    if (g_instance == NULL) {
        return 0;
    }
    if (!hasciicam_render_frame(g_instance)) {
        return 0;
    }
    if (!hasciicam_get_ascii_frame(g_instance, &g_last_text, NULL, &g_last_width, &g_last_height)) {
        return 0;
    }
    return 1;
}

const char *hasciicam_wasm_ascii_text(void) {
    return g_last_text;
}

int hasciicam_wasm_ascii_width(void) {
    return g_last_width;
}

int hasciicam_wasm_ascii_height(void) {
    return g_last_height;
}

void hasciicam_wasm_shutdown(void) {
    if (g_instance != NULL) {
        hasciicam_destroy(g_instance);
        g_instance = NULL;
    }
    g_last_text = NULL;
    g_last_width = 0;
    g_last_height = 0;
}
