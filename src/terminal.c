#include "terminal.h"
#include <string.h>
#include <stdio.h>

static void clear_line(Terminal *t,int y,int x0,int x1) {
    if(y<0||y>=t->rows)return;
    if(x0<0)x0=0;
    if(x1>=t->cols)x1=t->cols-1;
    for(int x=x0;x<=x1;x++){
        t->cells[y][x]=' ';t->fg[y][x]=t->fg_color;t->bg[y][x]=t->bg_color;t->attr[y][x]=(uint8_t)(t->reverse?1:0);
    }
    t->dirty=1;
}
static void clear_all(Terminal *t){for(int y=0;y<t->rows;y++)clear_line(t,y,0,t->cols-1);}
void term_init(Terminal *t){
    memset(t,0,sizeof(*t));
    t->cols=TERM_COLS;t->rows=TERM_ROWS;
    t->scroll_bottom=t->rows-1;t->fg_color=TERM_COLOR_DEFAULT;t->bg_color=TERM_COLOR_DEFAULT;
    t->cursor_visible=1;t->autowrap=1;
    clear_all(t);
}
int term_set_size(Terminal *t,int cols,int rows){
    if(cols<1||cols>TERM_MAX_COLS||rows<1||rows>TERM_MAX_ROWS)return 0;
    term_init(t);t->cols=cols;t->rows=rows;t->scroll_bottom=rows-1;
    clear_all(t);return 1;
}
static void clamp(Terminal *t){
    if(t->x<0)t->x=0;
    if(t->x>=t->cols)t->x=t->cols-1;
    if(t->y<0)t->y=0;
    if(t->y>=t->rows)t->y=t->rows-1;
}
static void scroll_up(Terminal *t,int n){
    int top=t->scroll_top,bot=t->scroll_bottom,len=bot-top+1;
    if(n>len)n=len;
    if(n<1)return;
    if(n<len){
        memmove(t->cells[top],t->cells[top+n],(size_t)(len-n)*sizeof(t->cells[0]));
        memmove(t->fg[top],t->fg[top+n],(size_t)(len-n)*sizeof(t->fg[0]));
        memmove(t->bg[top],t->bg[top+n],(size_t)(len-n)*sizeof(t->bg[0]));
        memmove(t->attr[top],t->attr[top+n],(size_t)(len-n)*TERM_MAX_COLS);
    }
    for(int y=bot-n+1;y<=bot;y++)clear_line(t,y,0,t->cols-1);
    t->dirty=1;
}
static void scroll_down(Terminal *t,int n){
    int top=t->scroll_top,bot=t->scroll_bottom,len=bot-top+1;
    if(n>len)n=len;
    if(n<1)return;
    if(n<len){
        memmove(t->cells[top+n],t->cells[top],(size_t)(len-n)*sizeof(t->cells[0]));
        memmove(t->fg[top+n],t->fg[top],(size_t)(len-n)*sizeof(t->fg[0]));
        memmove(t->bg[top+n],t->bg[top],(size_t)(len-n)*sizeof(t->bg[0]));
        memmove(t->attr[top+n],t->attr[top],(size_t)(len-n)*TERM_MAX_COLS);
    }
    for(int y=top;y<top+n;y++)clear_line(t,y,0,t->cols-1);
    t->dirty=1;
}
static void index_down(Terminal *t){
    if(t->y==t->scroll_bottom)scroll_up(t,1);
    else if(t->y<t->rows-1)t->y++;
    t->wrap_pending=0;
}
static void reverse_index(Terminal *t){
    if(t->y==t->scroll_top)scroll_down(t,1);
    else if(t->y>0)t->y--;
    t->wrap_pending=0;
}
static unsigned char decmap(unsigned char c){
    switch(c){case 'q':return TERM_HLINE;case 'x':return TERM_VLINE;
    case 'n':return TERM_CROSS;case 'l':return TERM_UL;case 'k':return TERM_UR;
    case 'm':return TERM_LL;case 'j':return TERM_LR;case 't':return TERM_TLEFT;
    case 'u':return TERM_TRIGHT;case 'w':return TERM_TTOP;case 'v':return TERM_TBOT;
    default:return c;}
}
/* Preserve Unicode code points rather than replacing unsupported symbols with '?'.
 * The renderer supplies procedural glyphs for box/block/braille characters. */
static uint32_t unimap(uint32_t cp){
    switch(cp){
    case 0x2500:case 0x2501:case 0x2550:return TERM_HLINE;
    case 0x2502:case 0x2503:case 0x2551:return TERM_VLINE;
    case 0x253c:case 0x256c:return TERM_CROSS;
    case 0x250c:case 0x2554:return TERM_UL;
    case 0x2510:case 0x2557:return TERM_UR;
    case 0x2514:case 0x255a:return TERM_LL;
    case 0x2518:case 0x255d:return TERM_LR;
    case 0x251c:case 0x2560:return TERM_TLEFT;
    case 0x2524:case 0x2563:return TERM_TRIGHT;
    case 0x252c:case 0x2566:return TERM_TTOP;
    case 0x2534:case 0x2569:return TERM_TBOT;
    default:return cp;
    }
}
static void putchar_t(Terminal *t,uint32_t c){
    if(t->wrap_pending){
        t->wrap_pending=0;
        if(t->autowrap){t->x=0;index_down(t);}
    }
    t->cells[t->y][t->x]=c;
    t->fg[t->y][t->x]=(t->bright && t->fg_color<8)?t->fg_color+8:t->fg_color;
    t->bg[t->y][t->x]=t->bg_color;
    t->attr[t->y][t->x]=(uint8_t)(t->reverse?1:0);
    t->dirty=1;
    if(t->x==t->cols-1)t->wrap_pending=t->autowrap;
    else t->x++;
}
static void alt_screen(Terminal *t,int enable){
    if(enable&&!t->alternate){
        memcpy(t->primary_cells,t->cells,sizeof(t->cells));
        memcpy(t->primary_fg,t->fg,sizeof(t->fg));
        memcpy(t->primary_bg,t->bg,sizeof(t->bg));
        memcpy(t->primary_attr,t->attr,sizeof(t->attr));
        t->primary_x=t->x;t->primary_y=t->y;
        t->alternate=1;
        t->x=t->y=0;
        t->scroll_top=0;t->scroll_bottom=t->rows-1;
        t->wrap_pending=0;
        clear_all(t);
    } else if(!enable&&t->alternate){
        memcpy(t->cells,t->primary_cells,sizeof(t->cells));
        memcpy(t->fg,t->primary_fg,sizeof(t->fg));
        memcpy(t->bg,t->primary_bg,sizeof(t->bg));
        memcpy(t->attr,t->primary_attr,sizeof(t->attr));
        t->x=t->primary_x;t->y=t->primary_y;
        t->alternate=0;t->scroll_top=0;t->scroll_bottom=t->rows-1;
        t->wrap_pending=0;t->dirty=1;
        clamp(t);
    }
}
static void queue_cursor_response(Terminal *t){
    char b[40];int len=snprintf(b,sizeof(b),"\033[%d;%dR",t->y+1,t->x+1);
    if(len>0&&(unsigned)len<=sizeof(t->response)-t->response_len){
        memcpy(t->response+t->response_len,b,(size_t)len);
        t->response_len+=(unsigned)len;
    }
}
static void csi(Terminal *t,unsigned char c){
    int p=t->params[0],q=t->params[1],n=p?p:1;
    t->wrap_pending=0;
    if(t->private_csi){
        if(c=='h'||c=='l'){
            int on=(c=='h');
            for(int i=0;i<=t->argc&&i<16;i++){
                int code=t->params[i];
                if(code==25)t->cursor_visible=on;
                else if(code==7)t->autowrap=on;
                else if(code==47||code==1047||code==1049)alt_screen(t,on);
            }
        }
        return;
    }
    switch(c){
    case 'A':t->y-=n;break;case 'B':t->y+=n;break;
    case 'C':t->x+=n;break;case 'D':t->x-=n;break;
    case 'E':t->y+=n;t->x=0;break;case 'F':t->y-=n;t->x=0;break;
    case 'G':t->x=n-1;break;case 'd':t->y=n-1;break;
    case 'H':case 'f':t->y=n-1;t->x=(q?q:1)-1;break;
    case 'J':
        if(p==2||p==3){clear_all(t);}
        else if(p==0){clear_line(t,t->y,t->x,t->cols-1);for(int y=t->y+1;y<t->rows;y++)clear_line(t,y,0,t->cols-1);}
        else if(p==1){for(int y=0;y<t->y;y++)clear_line(t,y,0,t->cols-1);clear_line(t,t->y,0,t->x);}
        break;
    case 'K':clear_line(t,t->y,p==1?0:(p==2?0:t->x),p==1?t->x:t->cols-1);break;
    case 'X':clear_line(t,t->y,t->x,t->x+n-1);break;
    case '@':{
        int cnt=n;if(cnt>t->cols-t->x)cnt=t->cols-t->x;
        if(cnt>0){size_t len=(size_t)(t->cols-t->x-cnt);
        memmove(t->cells[t->y]+t->x+cnt,t->cells[t->y]+t->x,len*sizeof(uint32_t));
        memmove(t->fg[t->y]+t->x+cnt,t->fg[t->y]+t->x,len*sizeof(uint32_t));
        memmove(t->bg[t->y]+t->x+cnt,t->bg[t->y]+t->x,len*sizeof(uint32_t));
        memmove(t->attr[t->y]+t->x+cnt,t->attr[t->y]+t->x,len);
        clear_line(t,t->y,t->x,t->x+cnt-1);}
        break;
    }
    case 'P':{
        int cnt=n;if(cnt>t->cols-t->x)cnt=t->cols-t->x;
        if(cnt>0){size_t len=(size_t)(t->cols-t->x-cnt);
        memmove(t->cells[t->y]+t->x,t->cells[t->y]+t->x+cnt,len*sizeof(uint32_t));
        memmove(t->fg[t->y]+t->x,t->fg[t->y]+t->x+cnt,len*sizeof(uint32_t));
        memmove(t->bg[t->y]+t->x,t->bg[t->y]+t->x+cnt,len*sizeof(uint32_t));
        memmove(t->attr[t->y]+t->x,t->attr[t->y]+t->x+cnt,len);
        clear_line(t,t->y,t->cols-cnt,t->cols-1);}
        break;
    }
    case 'L':if(t->y>=t->scroll_top&&t->y<=t->scroll_bottom){int top=t->scroll_top;t->scroll_top=t->y;scroll_down(t,n);t->scroll_top=top;}break;
    case 'M':if(t->y>=t->scroll_top&&t->y<=t->scroll_bottom){int top=t->scroll_top;t->scroll_top=t->y;scroll_up(t,n);t->scroll_top=top;}break;
    case 'S':scroll_up(t,n);break;case 'T':scroll_down(t,n);break;
    case 'r':{
        int bottom=q?q:t->rows;
        if(n>=1&&bottom<=t->rows&&n<bottom){t->scroll_top=n-1;t->scroll_bottom=bottom-1;t->x=t->y=0;}
        break;
    }
    case 'm':
        for(int i=0;i<=t->argc&&i<16;i++){
            int v=t->params[i];
            if(v==0){t->fg_color=TERM_COLOR_DEFAULT;t->bg_color=TERM_COLOR_DEFAULT;t->bright=0;t->reverse=0;}
            else if(v==1)t->bright=1;
            else if(v==22)t->bright=0;
            else if(v==7)t->reverse=1;
            else if(v==27)t->reverse=0;
            else if(v>=30&&v<=37)t->fg_color=(unsigned)(v-30);
            else if(v>=40&&v<=47)t->bg_color=(unsigned)(v-40);
            else if(v==39)t->fg_color=TERM_COLOR_DEFAULT;
            else if(v==49)t->bg_color=TERM_COLOR_DEFAULT;
            else if(v>=90&&v<=97)t->fg_color=(unsigned)(v-90+8);
            else if(v>=100&&v<=107)t->bg_color=(unsigned)(v-100+8);
            else if((v==38||v==48) && i+2<=t->argc){
                uint32_t color=TERM_COLOR_DEFAULT;
                int selector=t->params[++i];
                if(selector==5 && i+1<=t->argc){
                    int k=t->params[++i];if(k>=0&&k<256)color=(uint32_t)k;
                } else if(selector==2 && i+3<=t->argc){
                    int r=t->params[++i],g=t->params[++i],b=t->params[++i];
                    if(r>=0&&r<=255&&g>=0&&g<=255&&b>=0&&b<=255)
                        color=TERM_COLOR_RGB(r,g,b);
                }
                if(v==38)t->fg_color=color;else t->bg_color=color;
            }
        }
        break;
    case 's':t->saved_x=t->x;t->saved_y=t->y;break;
    case 'u':t->x=t->saved_x;t->y=t->saved_y;break;
    case 'n':if(p==6)queue_cursor_response(t);break;
    }
    clamp(t);
}
static void receive_byte(Terminal *t,unsigned char ch){
    if(t->state==1){
        t->state=0;
        if(ch=='['){t->state=2;t->argc=0;t->private_csi=0;memset(t->params,0,sizeof(t->params));}
        else if(ch==']')t->state=3;
        else if(ch=='('||ch==')')t->state=5;
        else if(ch=='7'){t->saved_x=t->x;t->saved_y=t->y;}
        else if(ch=='8'){t->x=t->saved_x;t->y=t->saved_y;clamp(t);}
        else if(ch=='D')index_down(t);
        else if(ch=='M')reverse_index(t);
        else if(ch=='E'){t->x=0;index_down(t);}
        else if(ch=='c'){int cols=t->cols,rows=t->rows;term_set_size(t,cols,rows);}
        return;
    }
    if(t->state==5){t->dec_graphics=(ch=='0');t->state=0;return;}
    if(t->state==3){if(ch==7)t->state=0;else if(ch==27)t->state=4;return;}
    if(t->state==4){t->state=(ch=='\\')?0:3;return;}
    if(t->state==2){
        if(ch=='?'&&t->argc==0){t->private_csi=1;return;}
        if(ch>='0'&&ch<='9'){
            if(t->argc<16&&t->params[t->argc]<10000)t->params[t->argc]=t->params[t->argc]*10+ch-'0';
            return;
        }
        if(ch==';'){if(t->argc<15)t->argc++;return;}
        if(ch>=0x40&&ch<=0x7e){csi(t,ch);t->state=0;return;}
        if(ch<0x20){t->state=0;return;}
        return;
    }
    if(ch==27){t->state=1;t->utf_remaining=0;return;}
    if(ch=='\r'){t->x=0;t->wrap_pending=0;return;}
    if(ch=='\n'||ch=='\v'||ch=='\f'){index_down(t);return;}
    if(ch=='\b'){if(t->wrap_pending)t->wrap_pending=0;else if(t->x>0)t->x--;return;}
    if(ch=='\t'){
        t->wrap_pending=0;
        t->x=(t->x+8)&~7;
        if(t->x>=t->cols)t->x=t->cols-1;
        return;
    }
    if(ch<32||ch==127)return;
    if(t->utf_remaining){
        if((ch&0xc0)==0x80){
            t->utf_code=(t->utf_code<<6)|(ch&0x3f);
            if(--t->utf_remaining==0)putchar_t(t,unimap(t->utf_code));
            return;
        }
        putchar_t(t,0xfffd);t->utf_remaining=0;
    }
    if(ch<128){putchar_t(t,t->dec_graphics?decmap(ch):ch);return;}
    if((ch&0xe0)==0xc0){t->utf_code=ch&0x1f;t->utf_remaining=1;return;}
    if((ch&0xf0)==0xe0){t->utf_code=ch&0x0f;t->utf_remaining=2;return;}
    if((ch&0xf8)==0xf0){t->utf_code=ch&0x07;t->utf_remaining=3;return;}
    putchar_t(t,0xfffd);
}
void term_feed(Terminal *t,const unsigned char *data,unsigned int n){
    for(unsigned int i=0;i<n;i++)receive_byte(t,data[i]);
}
unsigned int term_take_response(Terminal *t,unsigned char *output,unsigned int maxlen){
    unsigned int n=t->response_len;
    if(n>maxlen)n=maxlen;
    if(n){memcpy(output,t->response,n);t->response_len-=n;
       memmove(t->response,t->response+n,t->response_len);}
    return n;
}
