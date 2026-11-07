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

/* ANSI xterm palette: 16 ANSI colors, 6x6x6 cube, grayscale, and RGB. */
static XsshRgb resolve_color(XsshTheme theme,uint32_t index,int background){
    static const XsshRgb ansi16[16]={
        {0,0,0},{205,49,49},{13,188,121},{229,229,16},
        {36,114,200},{188,63,188},{17,168,205},{229,229,229},
        {102,102,102},{241,76,76},{35,209,139},{245,245,67},
        {59,142,234},{214,112,214},{41,184,219},{255,255,255}
    };
    if(index==0x02000000u){
        XsshThemeColors t=xssh_theme_colors(theme);
        return background?t.background:t.foreground;
    }
    if(index & 0x01000000u){
        XsshRgb v={(uint8_t)(index>>16),(uint8_t)(index>>8),(uint8_t)index};
        return v;
    }
    if(index<16)return ansi16[index];
    if(index<232){
        unsigned n=index-16;
        const uint8_t scale[6]={0,95,135,175,215,255};
        XsshRgb v={scale[n/36],scale[(n/6)%6],scale[n%6]};return v;
    }
    if(index<256){uint8_t v=(uint8_t)(8+10*(index-232));XsshRgb c={v,v,v};return c;}
    return background?xssh_theme_colors(theme).background:xssh_theme_colors(theme).foreground;
}
XsshRgb xssh_theme_terminal_color(XsshTheme theme,uint32_t color){return resolve_color(theme,color,0);}
XsshRgb xssh_theme_terminal_background(XsshTheme theme,uint32_t color){return resolve_color(theme,color,1);}
