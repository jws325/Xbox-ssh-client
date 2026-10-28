#include "font_size.h"
#include "terminal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void feed(Terminal *t,const char *s){term_feed(t,(const unsigned char*)s,(unsigned int)strlen(s));}
int main(void) {
    int size=0,w,h,cols,rows;
    assert(xssh_font_parse("12",&size)&&size==12);
    assert(xssh_font_parse("8",&size)&&size==8);
    assert(xssh_font_parse("24",&size)&&size==24);
    const char *invalid[]={"","0","7","25","12px","12.5","-12","+12","8 ","999999999999999999999999999"};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        int before=size;
        assert(!xssh_font_parse(invalid[i],&size));
        assert(size==before);
    }
    assert(!xssh_font_parse(NULL,&size));
    assert(!xssh_font_parse("12",NULL));
    for(int n=XSSH_FONT_MIN;n<=XSSH_FONT_MAX;n++){
        xssh_font_layout(n,&w,&h,&cols,&rows);
        assert(h==n && w>0 && cols>0 && rows>0);
        assert(w*cols<=XSSH_TERM_AREA_WIDTH);
        assert(h*rows<=XSSH_TERM_AREA_HEIGHT);
        assert(cols<=TERM_MAX_COLS&&rows<=TERM_MAX_ROWS);
        assert((cols+1)*w>XSSH_TERM_AREA_WIDTH || cols==TERM_MAX_COLS);
        assert((rows+1)*h>XSSH_TERM_AREA_HEIGHT || rows==TERM_MAX_ROWS);
    }
    xssh_font_layout(12,&w,&h,&cols,&rows);
    assert(w==8 && h==12 && cols==80 && rows==24);
    xssh_font_layout(8,&w,&h,&cols,&rows);
    assert(w==5 && h==8 && cols==128 && rows==36);
    xssh_font_layout(24,&w,&h,&cols,&rows);
    assert(w==16 && h==24 && cols==40 && rows==12);
    xssh_font_layout(1000,&w,&h,&cols,&rows);
    assert(w==8 && h==12 && cols==80 && rows==24);

    Terminal t;
    term_init(&t);
    assert(t.cols==80&&t.rows==24);
    assert(!term_set_size(&t,TERM_MAX_COLS+1,1));
    assert(!term_set_size(&t,1,0));
    assert(t.cols==80&&t.rows==24);
    assert(term_set_size(&t,128,36));
    assert(t.cols==128&&t.rows==36);
    feed(&t,"\033[36;127HZ");
    assert(t.cells[35][126]=='Z');
    assert(t.x==127&&t.y==35);
    feed(&t,"\033c");
    assert(t.cols==128&&t.rows==36);
    assert(t.cells[35][127]==' ');
    assert(term_set_size(&t,40,12));
    feed(&t,"\033[12;39HQ");
    assert(t.cols==40&&t.rows==12);
    assert(t.cells[11][38]=='Q');
    for(int i=0;i<2000;i++)feed(&t,"line\r\n");
    assert(t.x==0&&t.y==11);
    printf("font size and terminal grid tests passed\n");
    return 0;
}
