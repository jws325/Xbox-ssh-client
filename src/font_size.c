#include "font_size.h"
#include "terminal.h"
#include <limits.h>

int xssh_font_parse(const char *value, int *size) {
    int n=0;
    if(!value || !*value || !size) return 0;
    for(const char *p=value;*p;p++){
        if(*p<'0'||*p>'9'||n>(INT_MAX-9)/10)return 0;
        n=n*10+(*p-'0');
    }
    if(n<XSSH_FONT_MIN||n>XSSH_FONT_MAX)return 0;
    *size=n;
    return 1;
}

void xssh_font_layout_for_height(int size,int area_height,int *glyph_width,int *glyph_height,
                      int *columns, int *rows) {
    if(size<XSSH_FONT_MIN||size>XSSH_FONT_MAX)size=XSSH_FONT_DEFAULT;
    /* Preserve the original 8:12 aspect ratio as closely as integer
       logical coordinates allow. */
    int w=(size*2+1)/3;
    int cols=XSSH_TERM_AREA_WIDTH/w;
    int lines=area_height/size;
    if(cols>TERM_MAX_COLS)cols=TERM_MAX_COLS;
    if(lines>TERM_MAX_ROWS)lines=TERM_MAX_ROWS;
    if(glyph_width)*glyph_width=w;
    if(glyph_height)*glyph_height=size;
    if(columns)*columns=cols;
    if(rows)*rows=lines;
}

void xssh_font_layout(int size,int *glyph_width,int *glyph_height,int *columns,int *rows){
    xssh_font_layout_for_height(size,XSSH_TERM_AREA_HEIGHT,glyph_width,glyph_height,columns,rows);
}
