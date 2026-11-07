#ifndef XSSH_FONT_SIZE_H
#define XSSH_FONT_SIZE_H

/* Font sizes are bitmap-glyph heights in the 640x480 logical canvas.
 * 12 uses the original 8x12 glyphs. The UI remains fixed size; only the
 * terminal changes size, and its SSH PTY reflects the new grid. */
#define XSSH_FONT_DEFAULT 12
#define XSSH_FONT_MIN 8
#define XSSH_FONT_MAX 24
#define XSSH_TERM_AREA_WIDTH 640
#define XSSH_TERM_AREA_HEIGHT 288

/* Parse a decimal font height strictly; return zero for invalid settings. */
void xssh_font_layout_for_height(int size,int area_height,int *glyph_width,int *glyph_height,int *columns,int *rows);
int xssh_font_parse(const char *value, int *size);

/* Compute the dimensions of one glyph and of the terminal grid. */
void xssh_font_layout(int size, int *glyph_width, int *glyph_height,
                      int *columns, int *rows);

#endif
