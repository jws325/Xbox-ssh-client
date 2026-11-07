#ifndef XSSH_FONT_STYLE_H
#define XSSH_FONT_STYLE_H
#define XSSH_FONT_STYLES 4
#define XSSH_FONT_DEFAULT_STYLE 2
int xssh_font_style_parse(const char *s,int *out);
const char *xssh_font_style_name(int style);
#endif
