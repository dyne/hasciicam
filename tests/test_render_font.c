#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/render/render_font.h"

static int failed = 0;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        failed = 1;
    }
}

int main(void) {
    hasciicam_font_desc default_font;
    hasciicam_font_desc vga16;
    hasciicam_font_desc long_name;
    hasciicam_font_desc missing;
    FILE *fp;
    char line[256];
    int saw_vga16 = 0;
    int saw_courier = 0;

    expect_true(strcmp(hasciicam_font_default_name(), "vga16") == 0, "default font short name must be vga16");

    default_font = hasciicam_font_default();
    expect_true(default_font.font != 0, "default font must resolve");
    expect_true(default_font.short_name != 0 && strcmp(default_font.short_name, "vga16") == 0,
                "default descriptor short name must be vga16");

    expect_true(hasciicam_font_count() >= 10, "expected bundled AA font catalog");

    vga16 = hasciicam_font_find("vga16");
    expect_true(vga16.font != 0, "must find vga16");
    expect_true(vga16.height == 16, "vga16 height should be 16");

    long_name = hasciicam_font_find("Standard vga 8x16 font");
    expect_true(long_name.font != 0, "must find vga16 by long name");
    expect_true(long_name.short_name != 0 && strcmp(long_name.short_name, "vga16") == 0,
                "long-name lookup should resolve vga16");

    missing = hasciicam_font_find("no-such-font");
    expect_true(missing.font == 0, "unknown font should not resolve");

    fp = fopen("test_render_font_list.txt", "wb");
    expect_true(fp != 0, "must open temp file");
    if (fp != 0) {
        expect_true(hasciicam_font_write_list(fp) != 0, "font list writer should succeed");
        fclose(fp);
        fp = fopen("test_render_font_list.txt", "rb");
        expect_true(fp != 0, "must reopen temp file");
    }
    if (fp != 0) {
        while (fgets(line, (int)sizeof(line), fp) != 0) {
            if (strstr(line, "vga16\t") == line)
                saw_vga16 = 1;
            if (strstr(line, "courier\t") == line)
                saw_courier = 1;
        }
        fclose(fp);
    }
    remove("test_render_font_list.txt");

    expect_true(saw_vga16, "font list should include vga16");
    expect_true(saw_courier, "font list should include courier");

    if (failed) {
        return 1;
    }
    printf("render_font tests passed\n");
    return 0;
}
