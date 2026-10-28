#include "theme.h"
#include <string.h>

/* Bright Xbox green offers readable contrast against pure black. */
static const XsshRgb xbox_green = {0x9b, 0xf0, 0x0b};
static const XsshRgb pure_black = {0x00, 0x00, 0x00};

int xssh_theme_parse(const char *name, XsshTheme *out) {
    if (!name || !out) return 0;
    if (strcmp(name, "xbox") == 0) { *out = XSSH_THEME_XBOX; return 1; }
    if (strcmp(name, "inverted_xbox") == 0) { *out = XSSH_THEME_INVERTED_XBOX; return 1; }
    return 0;
}

const char *xssh_theme_name(XsshTheme theme) {
    return theme == XSSH_THEME_INVERTED_XBOX ? "inverted_xbox" : "xbox";
}

XsshThemeColors xssh_theme_colors(XsshTheme theme) {
    XsshThemeColors colors;
    if (theme == XSSH_THEME_INVERTED_XBOX) {
        colors.foreground = pure_black;
        colors.background = xbox_green;
    } else {
        colors.foreground = xbox_green;
        colors.background = pure_black;
    }
    return colors;
}

XsshRgb xssh_theme_terminal_color(XsshTheme theme, unsigned int ansi_color) {
    /* Preserve ANSI cursor movement and styling state, but render every glyph
       with the user's chosen foreground. Otherwise ANSI 30/31 etc. would
       break the requested two-color Xbox theme. */
    (void)ansi_color;
    return xssh_theme_colors(theme).foreground;
}
