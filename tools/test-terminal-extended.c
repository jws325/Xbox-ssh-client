#include "terminal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void feed(Terminal*t,const char*s){term_feed(t,(const unsigned char*)s,(unsigned)strlen(s));}
int main(void){
    Terminal t;term_init(&t);
    assert(term_set_size(&t,91,42));
    /* Right margin remains pending until the following printable glyph. */
    feed(&t,"\033[1;91HA");assert(t.x==90&&t.y==0&&t.wrap_pending);
    feed(&t,"\rB");assert(t.x==1&&t.y==0&&t.cells[0][0]=='B');
    feed(&t,"\033[1;91HC");feed(&t,"D");assert(t.cells[0][90]=='C'&&t.cells[1][0]=='D');
    /* Region-scoped scroll. */
    feed(&t,"\033[2;4r\033[4;1HZ\n");assert(t.y==3);
    /* Alternate screen restores original characters. */
    feed(&t,"\033[?1049h");assert(t.alternate&&t.cells[0][0]==' ');
    feed(&t,"hello\033[?1049l");assert(!t.alternate&&t.cells[0][0]=='B');
    /* Device status report returns row/col to caller. */
    feed(&t,"\033[3;8H\033[6n");
    unsigned char response[64]={0};unsigned n=term_take_response(&t,response,sizeof(response));
    assert(n==6&&memcmp(response,"\033[3;8R",6)==0);
    /* UTF-8 line drawing and DEC special graphics. */
    feed(&t,"\033[1;1H\342\224\200");assert(t.cells[0][0]==TERM_HLINE);
    feed(&t,"\033(0xq\033(B");assert(t.cells[0][1]==TERM_VLINE&&t.cells[0][2]==TERM_HLINE);
    /* Stress split ANSI sequences and a high-rate stream of updates. */
    for(int i=0;i<30000;i++)feed(&t,"\033[15;15H\033[32mX\033[0m\033[10;10H\033[1K");
    assert(t.cols==91&&t.rows==42);
    puts("extended terminal tests passed");return 0;
}
