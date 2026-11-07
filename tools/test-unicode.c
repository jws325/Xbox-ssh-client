#include "terminal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void feed(Terminal *t, const char *s) { term_feed(t,(const unsigned char*)s,(unsigned)strlen(s)); }
int main(void){
 Terminal t;term_init(&t);assert(term_set_size(&t,80,35));
 /* btop Braille graph points U+2801, U+28FF */
 feed(&t,"\342\240\201\342\243\277");
 assert(t.cells[0][0]==0x2801 && t.cells[0][1]==0x28ff);
 /* Split multibyte character across SSH read calls */
 const unsigned char seq[]={0xe2,0x96,0x88};
 term_feed(&t,seq,1);term_feed(&t,seq+1,2);
 assert(t.cells[0][2]==0x2588);
 /* Full block preserves codepoint instead of rendering #. */
 assert(t.cells[0][2]==0x2588);
 /* Box line character remains compatible with existing vector renderer. */
 feed(&t,"\342\224\200");assert(t.cells[0][3]==TERM_HLINE);
 /* Scroll must preserve full 32-bit codepoints. */
 feed(&t,"\033[35;1H\342\243\277\n");
 assert(t.cells[33][0]==0x28ff);
 /* Shift full-width Unicode codepoints with CSI insert/delete. */
 feed(&t,"\033[1;1H\342\243\277\033[1G\033[1@");
 assert(t.cells[0][1]==0x28ff);
 puts("Unicode terminal tests passed");return 0;
}
