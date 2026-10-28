#include "terminal.h"
#include <string.h>

static void clear_line(Terminal *t, int y, int first, int end) {
    if (y < 0 || y >= t->rows) return;
    if (first < 0) first=0;
    if (end >= t->cols) end=t->cols-1;
    for(int x=first;x<=end;x++) { t->cells[y][x]=' '; t->fg[y][x]=7; }
}
static void clear_all(Terminal *t) {
    for(int y=0;y<t->rows;y++) clear_line(t,y,0,t->cols-1);
}
void term_init(Terminal *t) {
    memset(t,0,sizeof(*t));
    t->cols=TERM_COLS;
    t->rows=TERM_ROWS;
    t->fg_color=7;
    t->cursor_visible=1;
    clear_all(t);
}
int term_set_size(Terminal *t, int cols, int rows) {
    if(cols<1||cols>TERM_MAX_COLS||rows<1||rows>TERM_MAX_ROWS)return 0;
    /* A new size resets state and clears the new active terminal screen. */
    term_init(t);
    t->cols=cols;
    t->rows=rows;
    clear_all(t);
    return 1;
}
static void scroll(Terminal *t) {
    memmove(t->cells[0],t->cells[1],(t->rows-1)*TERM_MAX_COLS);
    memmove(t->fg[0],t->fg[1],(t->rows-1)*TERM_MAX_COLS);
    clear_line(t,t->rows-1,0,t->cols-1);
}
static void newline(Terminal *t) { if (++t->y == t->rows) { scroll(t);t->y=t->rows-1; } }
static void clamp(Terminal *t) {
    if(t->x<0)t->x=0;
    if(t->x>=t->cols)t->x=t->cols-1;
    if(t->y<0)t->y=0;
    if(t->y>=t->rows)t->y=t->rows-1;
}
static void csi(Terminal *t,unsigned char c) {
    int p=t->params[0], q=t->params[1];
    if(!p && (c=='A'||c=='B'||c=='C'||c=='D')) p=1;
    switch(c) {
    case 'A': t->y-=p; break; case 'B': t->y+=p;break;
    case 'C': t->x+=p;break; case 'D': t->x-=p;break;
    case 'G': t->x=(p?p:1)-1;break;
    case 'd': t->y=(p?p:1)-1;break;
    case 'H': case 'f': t->y=(p?p:1)-1; t->x=(q?q:1)-1;break;
    case 'J':
        if(p==2||p==3) {clear_all(t); if(p==2) {t->x=0;t->y=0;}}
        else if(p==0) { clear_line(t,t->y,t->x,t->cols-1); for(int y=t->y+1;y<t->rows;y++)clear_line(t,y,0,t->cols-1);}
        else if(p==1) {for(int y=0;y<t->y;y++)clear_line(t,y,0,t->cols-1); clear_line(t,t->y,0,t->x);}
        break;
    case 'K': clear_line(t,t->y,p==1?0:(p==2?0:t->x),p==1?t->x:t->cols-1);break;
    case 'm':
        for (int i=0;i<=t->argc && i<8;i++) {
            int v=t->params[i];
            if(v==0) {t->fg_color=7;t->bright=0;}
            else if(v==1)t->bright=1;
            else if(v==22)t->bright=0;
            else if(v>=30&&v<=37)t->fg_color=v-30;
            else if(v==39)t->fg_color=7;
            else if(v>=90&&v<=97){t->fg_color=v-90;t->bright=1;}
        }
        break;
    case 's':t->saved_x=t->x;t->saved_y=t->y;break;
    case 'u':t->x=t->saved_x;t->y=t->saved_y;break;
    case 'h':if(t->private_csi&&p==25)t->cursor_visible=1;break;
    case 'l':if(t->private_csi&&p==25)t->cursor_visible=0;break;
    }
    clamp(t);
}
void term_feed(Terminal *t,const unsigned char *data,unsigned int n) {
    for(unsigned int i=0;i<n;i++) {
        unsigned char ch=data[i];
        if(t->state==1){
            if(ch=='[') { t->state=2;t->argc=0;t->private_csi=0;memset(t->params,0,sizeof(t->params)); }
            else if(ch=='7'){t->saved_x=t->x;t->saved_y=t->y;t->state=0;}
            else if(ch=='8'){t->x=t->saved_x;t->y=t->saved_y;clamp(t);t->state=0;}
            else if(ch=='c'){int cols=t->cols,rows=t->rows;term_set_size(t,cols,rows);t->state=0;}
            else if(ch==']')t->state=3;
            else t->state=0;
            continue;
        }
        if(t->state==3){if(ch==7)t->state=0;else if(ch==27)t->state=4;continue;}
        if(t->state==4){t->state=(ch=='\\')?0:3;continue;}
        if(t->state==2){
            if(ch=='?'){t->private_csi=1;continue;}
            if(ch>='0'&&ch<='9'){ if(t->argc<8 && t->params[t->argc]<10000)t->params[t->argc]=t->params[t->argc]*10+ch-'0';continue;}
            if(ch==';'){if(t->argc<7)t->argc++;continue;}
            if(ch>=0x40&&ch<=0x7e){csi(t,ch);t->state=0;continue;}
            if(ch<0x20){t->state=0;continue;}
            continue;
        }
        if(ch==27){t->state=1;continue;}
        if(ch=='\r'){t->x=0;continue;}
        if(ch=='\n'){newline(t);continue;}
        if(ch=='\b'){if(t->x)t->x--;continue;}
        if(ch=='\t'){t->x=(t->x+8)&~7; if(t->x>=t->cols){t->x=0;newline(t);}continue;}
        if(ch<32||ch==127)continue;
        /* Fallback for UTF-8 / non-ASCII; a minimal 7-bit terminal. */
        if(ch>126)ch='?';
        t->cells[t->y][t->x]=ch;
        t->fg[t->y][t->x]=(uint8_t)(t->fg_color+(t->bright?8:0));
        if(++t->x>=t->cols){t->x=0;newline(t);}
    }
}
