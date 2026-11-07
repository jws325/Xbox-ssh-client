#include "font_style.h"
#include <string.h>
static const char *const names[XSSH_FONT_STYLES]={"bold","regular","cousine","cousine_bold"};
int xssh_font_style_parse(const char *s,int *out){
    if(!s||!out)return 0;
    for(int i=0;i<XSSH_FONT_STYLES;i++)if(!strcmp(s,names[i])){*out=i;return 1;}
    return 0;
}
const char *xssh_font_style_name(int style){return (style>=0&&style<XSSH_FONT_STYLES)?names[style]:"invalid";}
