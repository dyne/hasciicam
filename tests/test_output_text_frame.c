#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#define test_mkdir(path) _mkdir(path)
#define test_rmdir(path) _rmdir(path)
#define test_path_exists(path) (_access((path), 0) == 0)
#else
#include <sys/stat.h>
#include <unistd.h>
#define test_mkdir(path) mkdir((path), 0700)
#define test_rmdir(path) rmdir(path)
#define test_path_exists(path) (access((path), F_OK) == 0)
#endif

#include "../src/output/output.h"
#include "../src/output/output_text_frame.h"

int hasciicam_capture_is_quiet(void) {
    return 1;
}

static int expect(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        return 0;
    }
    return 1;
}

static int read_file(const char *path, char *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t count;
    if (file == NULL)
        return 0;
    count = fread(buffer, 1, size, file);
    fclose(file);
    return (int)count;
}

int main(void) {
    const char text[] = {'A', ' ', ' ', 'B', 'C', ' '};
    const char expected[] = "A  \nBC \n";
    hasciicam_ascii_frame frame = {text, NULL, 3, 2};
    char error[128];
    char actual[1024];
    char pixels[64 * 32];
    hasciicam_render_session render;
    hasciicam_output output;
    hasciicam_ascii_frame rendered;
    int i;

    remove("test_output_text_frame.txt");
    remove("test_output_text_frame.txt.tmp");
    if (!expect(hasciicam_output_text_frame_write(&frame, "test_output_text_frame.txt", error, sizeof(error)),
                "exact text save failed")) return 1;
    memset(actual, 0, sizeof(actual));
    if (!expect(read_file("test_output_text_frame.txt", actual, sizeof(actual)) == (int)sizeof(expected) - 1 &&
                memcmp(actual, expected, sizeof(expected) - 1) == 0, "exact text bytes differ")) return 1;
    frame.text = "XYZ123";
    if (!expect(hasciicam_output_text_frame_write(&frame, "test_output_text_frame.txt", error, sizeof(error)),
                "overwrite failed")) return 1;
    memset(actual, 0, sizeof(actual));
    if (!expect(read_file("test_output_text_frame.txt", actual, sizeof(actual)) == 8 &&
                memcmp(actual, "XYZ\n123\n", 8) == 0, "overwrite bytes differ")) return 1;
    frame.text = text;
    if (!expect(!hasciicam_output_text_frame_write(NULL, "x", error, sizeof(error)), "null frame accepted") ||
        !expect(!hasciicam_output_text_frame_write(&frame, NULL, error, sizeof(error)), "null path accepted")) return 1;
    frame.width = 0;
    if (!expect(!hasciicam_output_text_frame_write(&frame, "x", error, sizeof(error)), "zero width accepted")) return 1;
    frame.width = 3;
    frame.height = 2147483647;
    if (!expect(!hasciicam_output_text_frame_write(&frame, "x", error, sizeof(error)), "overflow accepted")) return 1;
    frame.height = 2;
    remove("test_output_text_frame_dir.tmp");
    remove("test_output_text_frame_dir");
    if (!expect(test_mkdir("test_output_text_frame_dir") == 0, "cannot create test directory")) return 1;
    if (!expect(!hasciicam_output_text_frame_write(&frame, "test_output_text_frame_dir", error, sizeof(error)),
                "directory destination accepted") ||
        !expect(!test_path_exists("test_output_text_frame_dir.tmp"), "temporary file was not cleaned")) return 1;
    test_rmdir("test_output_text_frame_dir");

    for (i = 0; i < (int)sizeof(pixels); ++i)
        pixels[i] = (char)((i * 17) & 0xff);
    hasciicam_render_session_init(&render);
    hasciicam_render_session_configure_geometry(&render, 32, 16);
    render.hwparams.width = 32;
    render.hwparams.height = 16;
    render.context = aa_init(&mem_d, &render.hwparams, NULL);
    if (!expect(render.context != NULL && hasciicam_output_open_aalib(&output, render.context), "memory render setup failed")) return 1;
    memcpy(aa_image(render.context), pixels, sizeof(pixels));
    hasciicam_render_session_apply_tuning(&render, 70, 6, 2.0f);
    render.render_params->inversion = 1;
    hasciicam_output_write_ascii_frame_tuned(&output, 32, 16, render.render_params);
    if (!expect(hasciicam_render_session_get_ascii_frame(&render, &rendered), "rendered frame unavailable") ||
        !expect(hasciicam_output_text_frame_write(&rendered, "test_output_text_frame.txt", error, sizeof(error)),
                "rendered frame save failed")) return 1;
    memset(actual, 0, sizeof(actual));
    if (!expect(read_file("test_output_text_frame.txt", actual, sizeof(actual)) > 0, "rendered output empty")) return 1;
    for (i = 0; i < rendered.height; ++i) {
        if (!expect(memcmp(actual + i * (rendered.width + 1), rendered.text + i * rendered.width,
                           (size_t)rendered.width) == 0 &&
                    actual[i * (rendered.width + 1) + rendered.width] == '\n', "rendered text differs")) return 1;
    }
    hasciicam_output_close(&output);
    hasciicam_render_session_close(&render);
    remove("test_output_text_frame.txt");
    return 0;
}
