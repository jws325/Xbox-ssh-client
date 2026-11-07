#include "font_style.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){int x=-1; const char *names[]={"bold","regular","cousine","cousine_bold"};
for(int i=0;i<4;i++){assert(xssh_font_style_parse(names[i],&x)&&x==i);assert(!strcmp(xssh_font_style_name(i),names[i]));}
assert(!xssh_font_style_parse("nonexistent",&x));puts("Font styles OK");return 0;}
