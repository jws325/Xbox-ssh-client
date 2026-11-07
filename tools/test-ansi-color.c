#include "terminal.h"
#include "theme.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void feed(Terminal *t,const char *s){term_feed(t,(const unsigned char*)s,(unsigned)strlen(s));}
int main(void){
    Terminal t;term_init(&t);
    feed(&t,"\033[31;44mA\033[92;103mB");
    assert(t.fg[0][0]==1 && t.bg[0][0]==4);
    assert(t.fg[0][1]==10 && t.bg[0][1]==11);
    feed(&t,"\033[38;5;202;48;5;17mC");
    assert(t.fg[0][2]==202 && t.bg[0][2]==17);
    feed(&t,"\033[38;2;22;44;66;48;2;77;88;99mD");
    assert(t.fg[0][3]==TERM_COLOR_RGB(22,44,66));
    assert(t.bg[0][3]==TERM_COLOR_RGB(77,88,99));
    feed(&t,"\033[0mE");
    assert(t.fg[0][4]==TERM_COLOR_DEFAULT && t.bg[0][4]==TERM_COLOR_DEFAULT);
    feed(&t,"\033[1;1H\033[38;5;203mX\033[1G\033[1@ ");
    assert(t.fg[0][1]==203); /* shifted colors move with glyphs */
    puts("ANSI 16/256/RGB colors OK");return 0;
}
