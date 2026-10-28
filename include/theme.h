#ifndef XSSH_THEME_H
#define XSSH_THEME_H

#include <stdint.h>

typedef enum {
    XSSH_THEME_XBOX = 0,
    XSSH_THEME_INVERTED_XBOX = 1
} XsshTheme;

typedef struct {
    uint8_t r, g, b;
} XsshRgb;

typedef struct {
    XsshRgb foreground;
    XsshRgb background;
} XsshThemeColors;

/* "xbox" is the default. Returns 0 for unsupported names. */
int xssh_theme_parse(const char *name, XsshTheme *out);
const char *xssh_theme_name(XsshTheme theme);
/* Both themes are deliberately monochrome, including remote ANSI foregrounds. */
XsshThemeColors xssh_theme_colors(XsshTheme theme);
XsshRgb xssh_theme_terminal_color(XsshTheme theme, unsigned int ansi_color);

#endif
