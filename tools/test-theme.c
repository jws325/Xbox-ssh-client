#include "theme.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int same(XsshRgb a, XsshRgb b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}
int main(void) {
    XsshTheme t = XSSH_THEME_INVERTED_XBOX;
    XsshThemeColors xbox = xssh_theme_colors(XSSH_THEME_XBOX);
    XsshThemeColors inverted = xssh_theme_colors(XSSH_THEME_INVERTED_XBOX);
    assert(xssh_theme_parse("xbox", &t) && t == XSSH_THEME_XBOX);
    assert(xssh_theme_parse("inverted_xbox", &t) && t == XSSH_THEME_INVERTED_XBOX);
    assert(strcmp(xssh_theme_name(t), "inverted_xbox") == 0);
    assert(strcmp(xssh_theme_name(XSSH_THEME_XBOX), "xbox") == 0);
    assert(!xssh_theme_parse("unknown", &t));
    assert(!xssh_theme_parse("XBOX", &t));
    assert(!xssh_theme_parse("", &t));
    assert(!xssh_theme_parse(NULL, &t));
    assert(!xssh_theme_parse("xbox", NULL));
    assert(xbox.foreground.r == 0x9b && xbox.foreground.g == 0xf0 && xbox.foreground.b == 0x0b);
    assert(xbox.background.r == 0 && xbox.background.g == 0 && xbox.background.b == 0);
    assert(same(xbox.foreground, inverted.background));
    assert(same(xbox.background, inverted.foreground));
    assert(same(xssh_theme_terminal_color(XSSH_THEME_XBOX,0x02000000u),xbox.foreground));
    assert(same(xssh_theme_terminal_background(XSSH_THEME_XBOX,0x02000000u),xbox.background));
    assert(same(xssh_theme_terminal_color(XSSH_THEME_INVERTED_XBOX,0x02000000u),inverted.foreground));
    XsshRgb red=xssh_theme_terminal_color(XSSH_THEME_XBOX,9);
    assert(red.r==241&&red.g==76&&red.b==76);
    XsshRgb rgb=xssh_theme_terminal_color(XSSH_THEME_XBOX,0x01ffaa44u);
    assert(rgb.r==255&&rgb.g==170&&rgb.b==68);
    XsshRgb gray=xssh_theme_terminal_background(XSSH_THEME_XBOX,244);
    assert(gray.r==gray.g&&gray.g==gray.b);
    puts("theme tests passed");
    return 0;
}
