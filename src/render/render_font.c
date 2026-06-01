#include "render_font.h"

#include <string.h>

static hasciicam_font_desc font_desc_from_ptr(const struct aa_font *font) {
    hasciicam_font_desc desc;
    desc.short_name = 0;
    desc.name = 0;
    desc.height = 0;
    desc.font = 0;
    if (font == 0)
        return desc;
    desc.short_name = font->shortname;
    desc.name = font->name;
    desc.height = font->height;
    desc.font = font;
    return desc;
}

const char *hasciicam_font_default_name(void) {
    return "vga16";
}

hasciicam_font_desc hasciicam_font_default(void) {
    hasciicam_font_desc desc = hasciicam_font_find(hasciicam_font_default_name());
    if (desc.font != 0)
        return desc;
    return font_desc_from_ptr(&aa_font16);
}

int hasciicam_font_count(void) {
    int i;
    for (i = 0; aa_fonts[i] != 0; i++) {
    }
    return i;
}

hasciicam_font_desc hasciicam_font_at(int index) {
    int count = hasciicam_font_count();
    if (index < 0 || index >= count)
        return font_desc_from_ptr(0);
    return font_desc_from_ptr(aa_fonts[index]);
}

hasciicam_font_desc hasciicam_font_find(const char *name_or_short_name) {
    int i;
    if (name_or_short_name == 0 || name_or_short_name[0] == '\0')
        return font_desc_from_ptr(0);
    for (i = 0; aa_fonts[i] != 0; i++) {
        if ((aa_fonts[i]->shortname != 0 && strcmp(name_or_short_name, aa_fonts[i]->shortname) == 0) ||
            (aa_fonts[i]->name != 0 && strcmp(name_or_short_name, aa_fonts[i]->name) == 0)) {
            return font_desc_from_ptr(aa_fonts[i]);
        }
    }
    return font_desc_from_ptr(0);
}

int hasciicam_font_write_list(FILE *stream) {
    int i;
    if (stream == 0)
        return 0;
    for (i = 0; aa_fonts[i] != 0; i++) {
        const struct aa_font *font = aa_fonts[i];
        if (fprintf(stream, "%s\t%s\t8x%d\n",
                    font->shortname ? font->shortname : "",
                    font->name ? font->name : "",
                    font->height) < 0) {
            return 0;
        }
    }
    return 1;
}
