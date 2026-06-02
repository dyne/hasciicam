#ifndef HASCIICAM_RENDER_FONT_H
#define HASCIICAM_RENDER_FONT_H

#include <stdio.h>

#include <aalib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hasciicam_font_desc {
    const char *short_name;
    const char *name;
    int height;
    const struct aa_font *font;
} hasciicam_font_desc;

/* Returns canonical default AA font short name used by HasciiCam config/UI. */
const char *hasciicam_font_default_name(void);

/* Returns descriptor for the canonical default AA font. */
hasciicam_font_desc hasciicam_font_default(void);

/* Returns number of AA fonts currently registered in aa_fonts[]. */
int hasciicam_font_count(void);

/* Returns descriptor for font at index, or zeroed descriptor on invalid index. */
hasciicam_font_desc hasciicam_font_at(int index);

/* Finds font by short name or long name; returns zeroed descriptor if not found. */
hasciicam_font_desc hasciicam_font_find(const char *name_or_short_name);

/* Writes a stable font list to stream, one font per line; returns nonzero on success. */
int hasciicam_font_write_list(FILE *stream);

#ifdef __cplusplus
}
#endif

#endif
