#include "terminal.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
int main(void){
 Terminal t;
 term_init(&t);
 const char *s="hello\r\n\033[32mworld\033[0m";
 term_feed(&t,(const unsigned char*)s,strlen(s));
 assert(t.cells[0][0]=='h' && t.cells[1][0]=='w' && t.fg[1][0]==2);
 const char *mv="\033[2;4H#";
 term_feed(&t,(const unsigned char*)mv,strlen(mv));
 assert(t.cells[1][3]=='#');
 const char *cl="\033[2J";
 term_feed(&t,(const unsigned char*)cl,strlen(cl));
 assert(t.cells[1][3]==' ');
 puts("terminal tests passed");
 return 0;
}
