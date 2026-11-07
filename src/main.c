/* XSSH - an SSH2 terminal client for the original Xbox / nxdk.
   Networking: nxdk lwIP; crypto: libssh2 + Mbed TLS.
   Passwords are entered at runtime only; first-use SSH host keys need confirmation. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <SDL.h>
#include <hal/video.h>
#include <xboxkrnl/xboxkrnl.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <libssh2.h>
#include <mbedtls/sha256.h>
#include "font8x12.h"
#include "font8x12_aa.h"
#include "unicode_glyphs.h"
#include "terminal.h"
#include "rng.h"
#include "usb_keyboard.h"
#include "video_xbox.h"
#include "network_config.h"
#include "network_xbox.h"
#include "theme.h"
#include "font_size.h"
#include "font_style.h"
#include "connect_form.h"
#include "known_hosts.h"
#include "xssh_build.h"
#include "xssh_led.h"
#include "xssh_saver.h"

#define WIDTH 640
#define HEIGHT 480
#define GLYPH_W 8
#define GLYPH_H 12
#define SCREEN_Y 24
#define USB_TERMINAL_HEIGHT 424
#define PAD_TERMINAL_HEIGHT 288
#define KEY_Y 342
#define KEY_W 44
#define KEY_H 23
#define KNOWN_HOSTS_FILE "D:\\xssh-known-hosts.txt"
#define TX_CAP 4096
#define SSH_READ_BURST_MAX 8
#define SSH_READ_TIME_BUDGET_MS 4
#define CONFIG_FILE "D:\\xssh.cfg"
#define SAVER_DEFAULT_FILE "D:\\screensaver.xsv"

typedef enum { ST_FORM, ST_VERIFY_HOST, ST_CONNECTED, ST_FAILED, ST_POWER_CONFIRM } Stage;
typedef struct {
    char host[80];
    int port;
    char user[80];
    char pin[65];
    char video_mode[48];
    XsshTheme theme;
    int theme_config_valid;
    int font_size;
    int font_style;
    int text_antialiasing;
    int font_config_valid;
    XsshNetworkConfig network;
    int network_config_valid;
    XsshLedMode led_mode;
    int led_valid;
    int screensaver_enabled;
    int screensaver_seconds;
    char screensaver_file[120];
} Settings;
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *font;
    SDL_Texture *font_aa;
    SDL_Texture *term_cache;
    uint32_t cached_cells[TERM_MAX_ROWS][TERM_MAX_COLS];
    unsigned char cached_attrs[TERM_MAX_ROWS][TERM_MAX_COLS];
    uint32_t cached_fg[TERM_MAX_ROWS][TERM_MAX_COLS];
    uint32_t cached_bg[TERM_MAX_ROWS][TERM_MAX_COLS];
    int cache_valid;
    int term_height;
    int font_resize_pending;
    int last_keyboard_present;
    unsigned int last_render_ms;
    SDL_GameController *pad;
    Terminal term;
    int terminal_glyph_w, terminal_glyph_h;
    Settings settings;
    XsshVideoActive video;
    XsshNetworkState network;
    unsigned int last_network_poll;
    Stage stage;
    XsshConnectForm form;
    char pending_hostkey[65];
    int shift;
    int key_col, key_row;
    char status[160];
    int sock;
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel;
    unsigned char outgoing[TX_CAP];
    size_t tx_len;
    unsigned int pad_old;
    int running;
    int ready;
    int menu_focus; /* 0=form fields, 1=startup actions */
    int menu_action; /* 0=connect, 1=exit, 2=reboot, 3=shutdown */
    int power_action;
    uint8_t last_led_pattern;
    int led_pattern_valid;
    unsigned int last_input_ms;
    unsigned int saver_start_ms;
    unsigned int saver_last_frame;
    int saver_active;
    XsshSaver saver;
    SDL_Texture *saver_texture;
    uint32_t *saver_rgba_buffer;
    int saver_rgb565_native;
    int swallow_wake_text;
} App;

/* Forward declaration: startup power actions call this before its definition. */
static void disconnect(App *a);

static const char *keyboard[4] = {
    "1234567890-=", "qwertyuiop[]\\", "asdfghjkl;'", "zxcvbnm,./"
};
static const char *keyboard_shift[4] = {
    "!@#$%^&*()_+", "QWERTYUIOP{}|", "ASDFGHJKL:\"", "ZXCVBNM<>?"
};
static const char *special[6] = {"SPACE","ENTER","BACK","TAB","ESC","CTRL-C"};
static void logline(App *a,const char *s) {
    term_feed(&a->term,(const unsigned char*)s,(unsigned int)strlen(s));
    term_feed(&a->term,(const unsigned char*)"\r\n",2);
}
static void status(App *a,const char *s) {
    snprintf(a->status,sizeof(a->status),"%s",s);
    logline(a,s);
}
static char *trim(char *s) {
    while(*s==' '||*s=='\t')s++;
    size_t n=strlen(s);
    while(n&&(s[n-1]=='\r'||s[n-1]=='\n'||s[n-1]==' '||s[n-1]=='\t'))s[--n]=0;
    return s;
}
static int load_settings(Settings *s) {
    memset(s,0,sizeof(*s));
    s->port=22;
    s->theme=XSSH_THEME_XBOX;
    s->theme_config_valid=1;
    s->font_size=XSSH_FONT_DEFAULT;
    s->font_style=XSSH_FONT_DEFAULT_STYLE;
    s->text_antialiasing=0;
    s->font_config_valid=1;
    xssh_network_default(&s->network);
    s->network_config_valid=1;
    s->led_mode=XSSH_LED_AUTO;
    s->led_valid=1;
    s->screensaver_enabled=1;
    s->screensaver_seconds=120;
    snprintf(s->screensaver_file,sizeof(s->screensaver_file),"%s",SAVER_DEFAULT_FILE);
    snprintf(s->video_mode,sizeof(s->video_mode),"auto");
    FILE *f=fopen(CONFIG_FILE,"rb");
    if(!f)return 0; /* Interactive connection form does not need server defaults. */
    char line[220];
    while(fgets(line,sizeof(line),f)) {
        char *p=trim(line);if(*p=='#'||!*p)continue;
        char *eq=strchr(p,'=');if(!eq)continue;*eq++=0;
        p=trim(p);eq=trim(eq);
        if(!strcmp(p,"host"))snprintf(s->host,sizeof(s->host),"%s",eq);
        else if(!strcmp(p,"user"))snprintf(s->user,sizeof(s->user),"%s",eq);
        else if(!strcmp(p,"port"))s->port=atoi(eq);
        else if(!strcmp(p,"hostkey_sha256"))snprintf(s->pin,sizeof(s->pin),"%s",eq);
        else if(!strcmp(p,"video_mode"))snprintf(s->video_mode,sizeof(s->video_mode),"%s",eq);
        else if(!strcmp(p,"font_style")){
            if(!xssh_font_style_parse(eq,&s->font_style))s->font_config_valid=0;
        }
        else if(!strcmp(p,"text_antialiasing")){
            if(!strcmp(eq,"yes")||!strcmp(eq,"true")||!strcmp(eq,"1"))s->text_antialiasing=1;
            else if(!strcmp(eq,"no")||!strcmp(eq,"false")||!strcmp(eq,"0"))s->text_antialiasing=0;
            else s->font_config_valid=0;
        }
        else if(!strcmp(p,"font_size")){
            if(!xssh_font_parse(eq,&s->font_size))s->font_config_valid=0;
        }
        else if(!strcmp(p,"theme")){
            if(!xssh_theme_parse(eq,&s->theme))s->theme_config_valid=0;
        }
        else if(!strcmp(p,"network_mode")) {
            if(!xssh_network_parse_mode(eq,&s->network.mode))s->network_config_valid=0;
        }
        else if(!strcmp(p,"network_ip"))snprintf(s->network.ip,sizeof(s->network.ip),"%s",eq);
        else if(!strcmp(p,"network_netmask"))snprintf(s->network.netmask,sizeof(s->network.netmask),"%s",eq);
        else if(!strcmp(p,"network_gateway"))snprintf(s->network.gateway,sizeof(s->network.gateway),"%s",eq);
        else if(!strcmp(p,"led_mode")){if(!xssh_led_parse(eq,&s->led_mode))s->led_valid=0;}
        else if(!strcmp(p,"screensaver_enabled")){
            if(!strcmp(eq,"yes")||!strcmp(eq,"true")||!strcmp(eq,"1"))s->screensaver_enabled=1;
            else if(!strcmp(eq,"no")||!strcmp(eq,"false")||!strcmp(eq,"0"))s->screensaver_enabled=0;
            else s->led_valid=0;
        }
        else if(!strcmp(p,"screensaver_seconds")){
            int v=atoi(eq);if(v>=10 && v<=3600)s->screensaver_seconds=v;
            else s->led_valid=0;
        }
        else if(!strcmp(p,"screensaver_file"))snprintf(s->screensaver_file,sizeof(s->screensaver_file),"%s",eq);
    }
    fclose(f);
    if(!s->led_valid)return -5;
    if(!s->theme_config_valid)return -3;
    if(!s->font_config_valid)return -4;
    /* Do not silently use DHCP if the user explicitly configured invalid
       static settings; that could put credentials on an unintended network. */
    if(!s->network_config_valid || !xssh_network_validate(&s->network,NULL,0))return -2;
    return 0;
}
static int init_font(App *a) {
    SDL_Surface *s=SDL_CreateRGBSurface(0,16*8,6*12*XSSH_FONT_STYLES,32,
                                       0x00ff0000,0x0000ff00,0x000000ff,0xff000000);
    if(!s)return -1;
    uint32_t white=SDL_MapRGBA(s->format,255,255,255,255);
    uint32_t blank=SDL_MapRGBA(s->format,255,255,255,0);
    if(SDL_MUSTLOCK(s))SDL_LockSurface(s);
    for(int y=0;y<s->h;y++){
        uint32_t *dst=(uint32_t*)((uint8_t*)s->pixels+y*s->pitch);
        for(int x=0;x<s->w;x++){
            int face=y/(6*12);
            int gy=y%(6*12);
            int glyph=(gy/12)*16+x/8;
            uint8_t bit=(glyph<95 && face<XSSH_FONT_STYLES)?xssh_fonts[face][glyph][gy%12]:0;
            dst[x]=(bit & (0x80u>>(x%8)))?white:blank;
        }
    }
    if(SDL_MUSTLOCK(s))SDL_UnlockSurface(s);
    a->font=SDL_CreateTextureFromSurface(a->renderer,s);
    SDL_FreeSurface(s);
    if(!a->font)return -1;
    SDL_SetTextureBlendMode(a->font,SDL_BLENDMODE_BLEND);
    /* Separate 8-bit coverage atlas: color tint is applied by SDL at draw time.
       The unmodified binary bitmap remains available with AA disabled. */
    s=SDL_CreateRGBSurface(0,16*8,6*12*XSSH_FONT_STYLES,32,
                           0x00ff0000,0x0000ff00,0x000000ff,0xff000000);
    if(!s)return -1;
    if(SDL_MUSTLOCK(s) && SDL_LockSurface(s)!=0){SDL_FreeSurface(s);return -1;}
    for(int y=0;y<s->h;y++){
        uint32_t *dst=(uint32_t*)((uint8_t*)s->pixels+y*s->pitch);
        int face=y/(6*12),gy=y%(6*12),glyph_row=gy/12,py=gy%12;
        for(int x=0;x<s->w;x++){
            int glyph=glyph_row*16+x/8;
            uint8_t alpha=(face<XSSH_FONT_STYLES && glyph<95)
                ? xssh_fonts_aa[face][glyph][py][x%8] : 0;
            dst[x]=SDL_MapRGBA(s->format,255,255,255,alpha);
        }
    }
    if(SDL_MUSTLOCK(s))SDL_UnlockSurface(s);
    a->font_aa=SDL_CreateTextureFromSurface(a->renderer,s);
    SDL_FreeSurface(s);
    if(!a->font_aa)return -1;
    SDL_SetTextureBlendMode(a->font_aa,SDL_BLENDMODE_BLEND);
    return 0;
}
static void rect(App *a,int x,int y,int w,int h,XsshRgb color) {
    SDL_Rect rc={x,y,w,h};
    SDL_SetRenderDrawColor(a->renderer,color.r,color.g,color.b,255);
    SDL_RenderFillRect(a->renderer,&rc);
}
static void outline(App *a,int x,int y,int w,int h,XsshRgb color) {
    SDL_Rect rc={x,y,w,h};
    SDL_SetRenderDrawColor(a->renderer,color.r,color.g,color.b,255);
    SDL_RenderDrawRect(a->renderer,&rc);
}
static void glyph_scaled(App *a,int x,int y,char c,XsshRgb color,int w,int h) {
    if(c<32||c>126)return;
    int n=(int)c-32;
    SDL_Rect src={(n%16)*8,(n/16)*12+a->settings.font_style*6*12,8,12};
    SDL_Rect dst={x,y,w,h};
    SDL_Texture *atlas=a->settings.text_antialiasing?a->font_aa:a->font;
    SDL_SetTextureColorMod(atlas,color.r,color.g,color.b);
    SDL_RenderCopy(a->renderer,atlas,&src,&dst);
}
static void glyph(App *a,int x,int y,char c,XsshRgb color) {
    glyph_scaled(a,x,y,c,color,GLYPH_W,GLYPH_H);
}
static void text_at(App *a,int x,int y,const char *s,XsshRgb color,int max) {
    for(int i=0;s[i]&&i<max;i++)glyph(a,x+i*8,y,s[i],color);
}
static int key_index(const App*a) {
    if(a->key_row==4) {
        int c=a->key_col;
        if(c<3)return 0; if(c<6)return 1;if(c<8)return 2;
        if(c<10)return 3;if(c<12)return 4;return 5;
    }
    return a->key_col;
}
static void keyboard_key(App *a,int x,int y,int w,int h,int selected,XsshThemeColors c) {
    /* A selected key swaps the two theme colors, without introducing any
       third accent color. Unselected keys get an outline for separation. */
    rect(a,x,y,w,h,selected?c.foreground:c.background);
    if(!selected)outline(a,x,y,w,h,c.foreground);
}
static void box_glyph(App *a,int px,int py,int w,int h,unsigned char ch,XsshRgb c){
    int mx=px+w/2,my=py+h/2;
    int left=(ch==TERM_HLINE||ch==TERM_CROSS||ch==TERM_UR||ch==TERM_LR||ch==TERM_TLEFT||ch==TERM_TTOP||ch==TERM_TBOT);
    int right=(ch==TERM_HLINE||ch==TERM_CROSS||ch==TERM_UL||ch==TERM_LL||ch==TERM_TRIGHT||ch==TERM_TTOP||ch==TERM_TBOT);
    int up=(ch==TERM_VLINE||ch==TERM_CROSS||ch==TERM_LL||ch==TERM_LR||ch==TERM_TLEFT||ch==TERM_TRIGHT||ch==TERM_TBOT);
    int down=(ch==TERM_VLINE||ch==TERM_CROSS||ch==TERM_UL||ch==TERM_UR||ch==TERM_TLEFT||ch==TERM_TRIGHT||ch==TERM_TTOP);
    SDL_SetRenderDrawColor(a->renderer,c.r,c.g,c.b,255);
    if(left)SDL_RenderDrawLine(a->renderer,px,my,mx,my);
    if(right)SDL_RenderDrawLine(a->renderer,mx,my,px+w-1,my);
    if(up)SDL_RenderDrawLine(a->renderer,mx,py,mx,my);
    if(down)SDL_RenderDrawLine(a->renderer,mx,my,mx,py+h-1);
}
/* Codepoint renderer: btop uses UTF-8 Braille graphs (U+2800..28FF),
 * block fractions (U+2580..259F), and Unicode line art. */
static void unicode_glyph(App *a,int px,int py,int w,int h,uint32_t cp,XsshRgb c){
    SDL_SetRenderDrawColor(a->renderer,c.r,c.g,c.b,255);
    if(cp>=0x2800 && cp<=0x28ff){
        const unsigned bits=cp-0x2800;
        /* Unicode Braille positions: left 1,2,3,7; right 4,5,6,8. */
        const unsigned masks[2][4]={{1,2,4,64},{8,16,32,128}};
        int dw=w/4;if(dw<1)dw=1;
        int dh=h/8;if(dh<1)dh=1;
        for(int col=0;col<2;col++)for(int row=0;row<4;row++){
            if(bits & masks[col][row]){
                SDL_Rect dot={px+(col?3*w/4:w/4)-dw/2,
                              py+(row*2+1)*h/8-dh/2,dw,dh};
                SDL_RenderFillRect(a->renderer,&dot);
            }
        }
        return;
    }
    if(cp>=0x2580 && cp<=0x259f){
        if(cp==0x2588){SDL_Rect r={px,py,w,h};SDL_RenderFillRect(a->renderer,&r);return;}
        if(cp==0x2580||cp==0x2584){SDL_Rect r={px,py+(cp==0x2584?h/2:0),w,h/2};SDL_RenderFillRect(a->renderer,&r);return;}
        /* Block elements. Partition each cell into an 8x8 microgrid. */
        const unsigned quadrants[10]={4,8,1,13,9,7,11,2,6,14};
        for(int ry=0;ry<8;ry++)for(int rx=0;rx<8;rx++){
            int on=0;
            if(cp==0x2580)on=(ry<4);
            else if(cp>=0x2581&&cp<=0x2587)on=(ry>=8-(int)(cp-0x2580));
            else if(cp==0x2588)on=1;
            else if(cp>=0x2589&&cp<=0x258f)on=(rx<(int)(0x2590-cp));
            else if(cp==0x2590)on=(rx>=4);
            else if(cp>=0x2591&&cp<=0x2593){int v=(int)(cp-0x2590);on=((rx+2*ry)%4)<v;}
            else if(cp==0x2594)on=(ry==0);
            else if(cp==0x2595)on=(rx==7);
            else if(cp>=0x2596){int q=(int)(cp-0x2596);int mask=quadrants[q];int quadrant=(ry<4?0:2)+(rx<4?0:1);on=(mask>>quadrant)&1;}
            if(on){
                int x0=px+rx*w/8,x1=px+(rx+1)*w/8;
                int y0=py+ry*h/8,y1=py+(ry+1)*h/8;
                SDL_Rect r={x0,y0,x1>x0?x1-x0:1,y1>y0?y1-y0:1};
                SDL_RenderFillRect(a->renderer,&r);
            }
        }
        return;
    }
    /* Additional line variants render as consistent straight line segments. */
    if(cp>=0x2500&&cp<=0x257f){
        int midx=px+w/2,midy=py+h/2;
        if(cp==0x2500||cp==0x2501||cp==0x2504||cp==0x2505||cp==0x2508||cp==0x2509||cp==0x254c||cp==0x254d)
            SDL_RenderDrawLine(a->renderer,px,midy,px+w-1,midy);
        else if(cp==0x2502||cp==0x2503||cp==0x2506||cp==0x2507||cp==0x250a||cp==0x250b||cp==0x254e||cp==0x254f)
            SDL_RenderDrawLine(a->renderer,midx,py,midx,py+h-1);
        else { SDL_RenderDrawLine(a->renderer,px,midy,px+w-1,midy);
               SDL_RenderDrawLine(a->renderer,midx,py,midx,py+h-1); }
        return;
    }
    /* A small pre-rasterized supplemental Unicode atlas for arrows/symbols. */
    for(unsigned i=0;i<XSSH_UNICODE_GLYPHS;i++)if(xssh_unicode_points[i]==cp){
        const uint8_t *bitmap=xssh_unicode_bits[i];
        for(int gy=0;gy<12;gy++)for(int gx=0;gx<8;gx++)if(bitmap[gy]&(0x80u>>gx)){
            int x0=px+gx*w/8,x1=px+(gx+1)*w/8;
            int y0=py+gy*h/12,y1=py+(gy+1)*h/12;
            SDL_Rect r={x0,y0,x1>x0?x1-x0:1,y1>y0?y1-y0:1};
            SDL_RenderFillRect(a->renderer,&r);
        }
        return;
    }
    glyph_scaled(a,px,py,'?',c,w,h);
}
static void render_terminal_glyph(App *a,int px,int py,int w,int h,uint32_t ch,XsshRgb fg){
    if(ch>=TERM_HLINE&&ch<=TERM_TBOT)box_glyph(a,px,py,w,h,(unsigned char)ch,fg);
    else if(ch>=32&&ch<=126)glyph_scaled(a,px,py,(char)ch,fg,w,h);
    else if(ch>126)unicode_glyph(a,px,py,w,h,ch,fg);
}
static void draw_terminal_cell(App *a,int x,int y,XsshThemeColors c){
    int px=x*a->terminal_glyph_w,py=y*a->terminal_glyph_h;
    uint32_t ch=a->term.cells[y][x];
    int rev=a->term.attr[y][x]&1;
    XsshRgb fg=xssh_theme_terminal_color(a->settings.theme,a->term.fg[y][x]);
    XsshRgb bg=xssh_theme_terminal_background(a->settings.theme,a->term.bg[y][x]);
    if(rev){XsshRgb tmp=fg;fg=bg;bg=tmp;}
    rect(a,px,py,a->terminal_glyph_w,a->terminal_glyph_h,bg);
    if(ch!=' ')render_terminal_glyph(a,px,py,a->terminal_glyph_w,a->terminal_glyph_h,ch,fg);
}
/* Render into an SDL target texture only when cells change. If the nxdk SDL2
   backend cannot target textures, draw directly as a compatibility fallback. */
static void draw_terminal(App *a,XsshThemeColors c){
    int cache_ready=a->term_cache!=NULL;
    if(cache_ready && SDL_SetRenderTarget(a->renderer,a->term_cache)!=0){
        SDL_DestroyTexture(a->term_cache);a->term_cache=NULL;
        a->cache_valid=0;cache_ready=0;
    }
    if(cache_ready){
        if(!a->cache_valid){
            SDL_SetRenderDrawColor(a->renderer,c.background.r,c.background.g,c.background.b,255);
            SDL_RenderClear(a->renderer);
        }
        if(a->term.dirty||!a->cache_valid){
            for(int y=0;y<a->term.rows;y++)for(int x=0;x<a->term.cols;x++){
                uint32_t ch=a->term.cells[y][x],fg=a->term.fg[y][x],bg=a->term.bg[y][x],attr=a->term.attr[y][x];
                if(!a->cache_valid || a->cached_cells[y][x]!=ch || a->cached_fg[y][x]!=fg || a->cached_bg[y][x]!=bg || a->cached_attrs[y][x]!=attr){
                    draw_terminal_cell(a,x,y,c);
                    a->cached_cells[y][x]=ch;a->cached_fg[y][x]=fg;a->cached_bg[y][x]=bg;a->cached_attrs[y][x]=attr;
                }
            }
            a->cache_valid=1;a->term.dirty=0;
        }
        SDL_SetRenderTarget(a->renderer,NULL);
        SDL_Rect from={0,0,WIDTH,a->term_height},to={0,SCREEN_Y,WIDTH,a->term_height};
        SDL_RenderCopy(a->renderer,a->term_cache,&from,&to);
    } else {
        for(int y=0;y<a->term.rows;y++)for(int x=0;x<a->term.cols;x++){
            /* Fallback: draw in target coordinates rather than cache coordinates. */
            int px=x*a->terminal_glyph_w,py=SCREEN_Y+y*a->terminal_glyph_h;
            uint32_t ch=a->term.cells[y][x];
            int rev=a->term.attr[y][x]&1;
            XsshRgb fg=xssh_theme_terminal_color(a->settings.theme,a->term.fg[y][x]);
            XsshRgb bg=xssh_theme_terminal_background(a->settings.theme,a->term.bg[y][x]);
            if(rev){XsshRgb tmp=fg;fg=bg;bg=tmp;}
            rect(a,px,py,a->terminal_glyph_w,a->terminal_glyph_h,bg);
            if(ch!=' ')render_terminal_glyph(a,px,py,a->terminal_glyph_w,a->terminal_glyph_h,ch,fg);
        }
        a->term.dirty=0;
    }
    if(a->term.cursor_visible){
        int h=a->terminal_glyph_h>=12?2:1;
        rect(a,a->term.x*a->terminal_glyph_w,SCREEN_Y+(a->term.y+1)*a->terminal_glyph_h-h,
             a->terminal_glyph_w,h,c.foreground);
    }
}
static void resize_for_keyboard(App *a,int force){
    int now=xssh_usb_keyboard_connected()!=0;
    if(!force && !a->font_resize_pending && now==a->last_keyboard_present)return;
    int width,rows;
    a->term_height=now?USB_TERMINAL_HEIGHT:PAD_TERMINAL_HEIGHT;
    xssh_font_layout_for_height(a->settings.font_size,a->term_height,&a->terminal_glyph_w,&a->terminal_glyph_h,&width,&rows);
    if(a->term.cols==width&&a->term.rows==rows){
        a->last_keyboard_present=now;
        a->font_resize_pending=0;
        if(force)a->cache_valid=0;
        return;
    }
    /* The server must receive the new grid so full-screen tools reflow. */
    if(a->channel && a->stage==ST_CONNECTED){
        int r=libssh2_channel_request_pty_size(a->channel,width,rows);
        if(r==LIBSSH2_ERROR_EAGAIN){
            /* Retry on the next pump; don't send an incorrect size. */
            return;
        }
        if(r!=0)return;
    }
    term_set_size(&a->term,width,rows);
    a->last_keyboard_present=now;
    a->cache_valid=0;
    a->font_resize_pending=0;
}
/* Keep the terminal's pixel dimensions and server PTY in sync. */
static void change_font_size(App *a,int delta){
    int next=a->settings.font_size+delta;
    if(next<XSSH_FONT_MIN)next=XSSH_FONT_MIN;
    if(next>XSSH_FONT_MAX)next=XSSH_FONT_MAX;
    if(next==a->settings.font_size)return;
    a->settings.font_size=next;
    a->font_resize_pending=1;
    resize_for_keyboard(a,1);
}
static void change_font_style(App *a){
    a->settings.font_style=(a->settings.font_style+1)%XSSH_FONT_STYLES;
    a->cache_valid=0;
    a->term.dirty=1;
    snprintf(a->status,sizeof(a->status),"Font: %s (Ctrl+F to cycle)",
             xssh_font_style_name(a->settings.font_style));
}
/* Runtime override: leave the on-disk xssh.cfg preference untouched. */
static void toggle_text_antialiasing(App *a){
    a->settings.text_antialiasing=!a->settings.text_antialiasing;
    a->cache_valid=0;
    a->term.dirty=1;
    snprintf(a->status,sizeof(a->status),"Text antialiasing: %s (Ctrl+A)",
             a->settings.text_antialiasing?"ON":"OFF");
}
static void startup_action(App *a,int selected){
    if(selected==0){a->menu_focus=0;a->form.field=XSSH_FORM_HOST;return;}
    if(selected==1){a->running=0;return;}
    a->power_action=selected;
    a->stage=ST_POWER_CONFIRM;
    snprintf(a->status,sizeof(a->status),"Press Y to confirm, N or Esc to cancel");
}
static void confirm_power(App *a,int yes){
    if(!yes){a->stage=ST_FORM;return;}
    /* HalReturnToFirmware(0) halts, it does NOT power off this Xbox.
       Power-off is controlled by the SMC at 8-bit SMBus addr 0x20,
       power register 0x02, command 0x80.  Do not issue a reboot
       as a fallback on shutdown failure. */
    const int shutdown=(a->power_action==3);
    disconnect(a);
    if(shutdown){
        NTSTATUS result=HalWriteSMBusValue(0x20,0x02,FALSE,0x80);
        if((int32_t)result<0){
            a->stage=ST_FORM;
            status(a,"SMC power-off command failed; Xbox was NOT rebooted");
            return;
        }
        /* Allow the SMC to remove power before resuming the application.
           A failed power-off is reported, not silently changed to reboot. */
        unsigned int start=SDL_GetTicks();
        while((unsigned)(SDL_GetTicks()-start)<1500)SDL_Delay(10);
        a->stage=ST_FORM;
        status(a,"SMC acknowledged shutdown, but Xbox is still running");
        return;
    }
    if(a->term_cache){SDL_DestroyTexture(a->term_cache);a->term_cache=NULL;}
    if(a->font){SDL_DestroyTexture(a->font);a->font=NULL;}
    if(a->font_aa){SDL_DestroyTexture(a->font_aa);a->font_aa=NULL;}
    if(a->pad){SDL_GameControllerClose(a->pad);a->pad=NULL;}
    if(a->renderer){SDL_DestroyRenderer(a->renderer);a->renderer=NULL;}
    if(a->window){SDL_DestroyWindow(a->window);a->window=NULL;}
    xssh_led_restore();
    SDL_Quit();
    HalReturnToFirmware(1); /* HalRebootRoutine */
    a->running=0;
}
static void update_front_led(App *a){
    int state=a->saver_active?4:a->stage==ST_CONNECTED?1:
              a->stage==ST_VERIFY_HOST?2:a->stage==ST_FAILED?3:0;
    uint8_t pattern=xssh_led_pattern(a->settings.led_mode,state);
    if(!a->led_pattern_valid || a->last_led_pattern!=pattern){
        xssh_led_apply(pattern);
        a->led_pattern_valid=1;a->last_led_pattern=pattern;
    }
}
static void saver_stop(App *a){
    if(!a->saver_active)return;
    if(a->saver_texture){SDL_DestroyTexture(a->saver_texture);a->saver_texture=NULL;}
    free(a->saver_rgba_buffer);a->saver_rgba_buffer=NULL;
    xssh_saver_close(&a->saver);
    a->saver_active=0;
    a->cache_valid=0;
    a->term.dirty=1;
    a->last_render_ms=0;
    update_front_led(a);
}
static void saver_start(App *a,unsigned int now){
    if(!a->settings.screensaver_enabled || a->saver_active || a->stage==ST_POWER_CONFIRM ||
       a->stage==ST_VERIFY_HOST)return;
    if(!xssh_saver_open(&a->saver,a->settings.screensaver_file)){
        /* Retry after another timeout, not on every 2ms loop iteration. */
        a->last_input_ms=now;
        return;
    }
    a->saver_rgb565_native=1;
    a->saver_texture=SDL_CreateTexture(a->renderer,SDL_PIXELFORMAT_RGB565,
                       SDL_TEXTUREACCESS_STREAMING,(int)a->saver.width,(int)a->saver.height);
    if(!a->saver_texture){
        /* Some SDL backends expose only 32-bit textures. Conversion is a
           fallback, not required for the normal fast RGB565 path. */
        a->saver_rgb565_native=0;
        a->saver_texture=SDL_CreateTexture(a->renderer,SDL_PIXELFORMAT_RGBA8888,
                           SDL_TEXTUREACCESS_STREAMING,(int)a->saver.width,(int)a->saver.height);
        a->saver_rgba_buffer=(uint32_t*)malloc((size_t)a->saver.width*a->saver.height*4);
        if(!a->saver_rgba_buffer){if(a->saver_texture)SDL_DestroyTexture(a->saver_texture);
            a->saver_texture=NULL;}
    }
    if(!a->saver_texture){free(a->saver_rgba_buffer);a->saver_rgba_buffer=NULL;
        xssh_saver_close(&a->saver);a->last_input_ms=now;return;}
    SDL_SetTextureBlendMode(a->saver_texture,SDL_BLENDMODE_NONE);
    a->saver_start_ms=now;a->saver_last_frame=UINT32_MAX;
    a->saver_active=1;update_front_led(a);
}
static void saver_draw(App *a,unsigned int now){
    if(!a->saver_active)return;
    uint32_t frame=((uint32_t)(now-a->saver_start_ms)*a->saver.fps/1000u)%a->saver.frames;
    if(frame!=a->saver_last_frame){
        if(!xssh_saver_frame(&a->saver,frame)){saver_stop(a);return;}
        const void *pixels=a->saver.pixels;
        int pitch=(int)a->saver.width*2;
        if(!a->saver_rgb565_native){
            size_t n=(size_t)a->saver.width*a->saver.height;
            for(size_t i=0;i<n;i++){
                uint16_t v=(uint16_t)a->saver.pixels[i*2]|((uint16_t)a->saver.pixels[i*2+1]<<8);
                uint32_t r=(v>>11)&31, g=(v>>5)&63, b=v&31;
                a->saver_rgba_buffer[i]=((r*255/31)<<24)|((g*255/63)<<16)|((b*255/31)<<8)|255;
            }
            pixels=a->saver_rgba_buffer;
            pitch=(int)a->saver.width*4;
        }
        if(SDL_UpdateTexture(a->saver_texture,NULL,pixels,pitch)!=0){saver_stop(a);return;}
        a->saver_last_frame=frame;
    }
    SDL_SetRenderDrawColor(a->renderer,0,0,0,255);
    SDL_RenderClear(a->renderer);
    SDL_Rect dest={0,0,WIDTH,HEIGHT};
    SDL_RenderCopy(a->renderer,a->saver_texture,NULL,&dest);
    SDL_RenderPresent(a->renderer);
}
static void render(App *a) {
    XsshThemeColors c=xssh_theme_colors(a->settings.theme);
    SDL_SetRenderDrawColor(a->renderer,c.background.r,c.background.g,c.background.b,255);
    SDL_RenderClear(a->renderer);
    text_at(a,8,6,XSSH_UI_LABEL,c.foreground,25);
    {char video_font[48];
     snprintf(video_font,sizeof(video_font),"%s F%d %s AA:%s",a->video.description,a->settings.font_size,
              xssh_font_style_name(a->settings.font_style),a->settings.text_antialiasing?"ON":"OFF");
     text_at(a,290,6,video_font,c.foreground,24);
    }
    const char *lab=(a->stage==ST_CONNECTED)?"SSH CONNECTED":
                    (a->stage==ST_VERIFY_HOST)?"VERIFY HOST KEY":
                    (a->stage==ST_FORM)?"SSH LOGIN":(a->stage==ST_POWER_CONFIRM)?"POWER MENU":"DISCONNECTED";
    text_at(a,496,6,lab,c.foreground,18);
    rect(a,0,23,WIDTH,1,c.foreground);
    if(a->stage==ST_CONNECTED) {
        draw_terminal(a,c);
    } else if(a->stage==ST_POWER_CONFIRM) {
        text_at(a,72,100,a->power_action==3?"SHUT DOWN ORIGINAL XBOX?":"RESTART ORIGINAL XBOX?",c.foreground,60);
        text_at(a,72,150,"Y / Enter = CONFIRM",c.foreground,60);
        text_at(a,72,175,"N / Escape = CANCEL",c.foreground,60);
    } else if(a->stage==ST_VERIFY_HOST) {
        text_at(a,14,65,"NEW SSH SERVER - VERIFY FINGERPRINT",c.foreground,76);
        text_at(a,14,98,"Server SHA-256 host key (hex):",c.foreground,76);
        text_at(a,14,119,a->pending_hostkey,c.foreground,65);
        text_at(a,14,167,"Compare this fingerprint using a trusted source.",c.foreground,76);
        text_at(a,14,192,"B / Enter = TRUST & CONNECT",c.foreground,76);
        text_at(a,14,215,"X / Esc = CANCEL (no password sent)",c.foreground,76);
        text_at(a,14,246,"Trusted keys saved as D:\\xssh-known-hosts.txt",c.foreground,76);
    } else {
        text_at(a,14,47,"CONNECT TO SSH SERVER",c.foreground,60);
        /* Release identifier is visible before any network login. */
        text_at(a,387,47,XSSH_BUILD_ID,c.foreground,30);
        for(int field=0;field<XSSH_FORM_COUNT;field++) {
            const char *value=(field==XSSH_FORM_HOST)?a->form.host:
                              (field==XSSH_FORM_PORT)?a->form.port:
                              (field==XSSH_FORM_USER)?a->form.user:a->form.password;
            char display[80];
            if(field==XSSH_FORM_PASSWORD){
                size_t n=strlen(value);if(n>sizeof(display)-1)n=sizeof(display)-1;
                memset(display,'*',n);display[n]=0;
            }else snprintf(display,sizeof(display),"%s",value);
            int y=77+field*43;
            if(a->stage==ST_FORM && a->form.field==field) {
                rect(a,8,y-5,624,26,c.foreground);
                text_at(a,14,y,xssh_form_field_name(field),c.background,12);
                text_at(a,150,y,display,c.background,58);
                int cursor_x=150+(int)strlen(display)*8;
                if(cursor_x<615)rect(a,cursor_x,y+11,7,2,c.background);
            } else {
                outline(a,8,y-5,624,26,c.foreground);
                text_at(a,14,y,xssh_form_field_name(field),c.foreground,12);
                text_at(a,150,y,display,c.foreground,58);
            }
        }
        const char *actions[]={"CONNECT","EXIT","RESTART","SHUTDOWN"};
        for(int i=0;i<4;i++){
            int x=14+i*156,y=270;
            int selected=(a->menu_focus && a->menu_action==i);
            if(selected)rect(a,x,y,144,25,c.foreground);
            else outline(a,x,y,144,25,c.foreground);
            text_at(a,x+12,y+7,actions[i],selected?c.background:c.foreground,18);
        }
        text_at(a,14,302,"TAB: MENU / F9: EXIT  F10: RESTART  F11: OFF",c.foreground,74);
    }
    int usb_terminal=(a->stage==ST_CONNECTED && a->last_keyboard_present);
    rect(a,0,usb_terminal?450:326,WIDTH,1,c.foreground);
    text_at(a,4,usb_terminal?451:329,a->status,c.foreground,78);
    /* USB keyboard auto-hide remains unchanged in both themes. */
    if(xssh_software_keyboard_visible()) {
        for(int row=0;row<5;row++){
            if(row<4){
                for(int col=0;col<14;col++){
                    const char *keys=a->shift?keyboard_shift[row]:keyboard[row];
                    if(col>=(int)strlen(keys))continue;
                    char ch=keys[col];
                    int x=12+col*KEY_W,y=KEY_Y+row*KEY_H;
                    int sel=(row==a->key_row&&col==a->key_col);
                    keyboard_key(a,x,y,41,21,sel,c);
                    glyph(a,x+17,y+4,ch,sel?c.background:c.foreground);
                }
            } else {
                static const int starts[]={0,3,6,8,10,12};
                static const int spans[]={3,3,2,2,2,2};
                for(int j=0;j<6;j++){
                    int x=12+starts[j]*KEY_W,y=KEY_Y+row*KEY_H;
                    int sel=(a->key_row==row&&key_index(a)==j);
                    int w=spans[j]*KEY_W-3;
                    keyboard_key(a,x,y,w,21,sel,c);
                    int chars=(int)strlen(special[j]);
                    int offset=(w-chars*8)/2;
                    if(offset<0)offset=0;
                    text_at(a,x+offset,y+4,special[j],sel?c.background:c.foreground,12);
                }
            }
        }
    } else if(a->stage!=ST_CONNECTED) {
        text_at(a,12,353,"USB KEYBOARD CONNECTED",c.foreground,52);
        text_at(a,12,379,"Type using your USB keyboard.",c.foreground,55);
        text_at(a,12,402,"Unplug to restore the controller keyboard.",c.foreground,55);
    }
    {char footer[100];
     snprintf(footer,sizeof(footer),"XBOX IP: %-15s | %s | %s",
              a->network.initialized ? a->network.address : "UNAVAILABLE",
              xssh_usb_keyboard_connected() ? "USB KB READY" : "USB KB NOT FOUND",
              xssh_software_keyboard_visible() ?
                  "D-PAD/A TYPE B ENTER X BACK" : "USB TYPE B ENTER X BACK");
     text_at(a,4,463,footer,c.foreground,79);}
    SDL_RenderPresent(a->renderer);
}

static void disconnect(App *a) {
    if(a->channel){
        /* Socket close first to avoid a blocking protocol shutdown. */
        if(a->sock>=0){close(a->sock);a->sock=-1;}
        libssh2_channel_free(a->channel);a->channel=NULL;
    }
    if(a->sock>=0){close(a->sock);a->sock=-1;}
    if(a->session){libssh2_session_free(a->session);a->session=NULL;}
    a->tx_len=0;
}
static int pin_is_valid(const char *pin) {
    if(strlen(pin)!=64)return 0;
    for(int i=0;i<64;i++)if(!isxdigit((unsigned char)pin[i]))return 0;
    return 1;
}
static int match_pin(const char *pin, const char *computed) {
    unsigned char diff=0;
    for(int i=0;i<64;i++)diff |= (unsigned char)(tolower((unsigned char)pin[i])^computed[i]);
    return diff==0;
}
static int ssh_auth_complete(App *a) {
    if(libssh2_userauth_password(a->session,a->form.user,a->form.password)) {
        status(a,"Password authentication failed");return -1;
    }
    a->channel=libssh2_channel_open_session(a->session);
    if(!a->channel){status(a,"Could not open SSH channel");return -1;}
    if(libssh2_channel_request_pty_ex(a->channel,"xterm",5,NULL,0,
                                       a->term.cols,a->term.rows,0,0)!=0){
        status(a,"PTY request failed");return -1;
    }
    if(libssh2_channel_shell(a->channel)!=0){status(a,"Shell request failed");return -1;}
    {unsigned long nonblocking=1;
     if(ioctlsocket(a->sock,FIONBIO,&nonblocking)!=0){
         status(a,"Cannot enable nonblocking socket");return -1;
     }}
    libssh2_session_set_blocking(a->session,0);
    term_set_size(&a->term,a->term.cols,a->term.rows);
    status(a,"Connected - remote shell ready");
    return 0;
}
/* 0 connected, 1 awaiting explicit fingerprint confirmation, -1 failed. */
static int ssh_connect(App *a) {
    struct sockaddr_in addr;
    const char *hostkey=NULL;
    size_t keylen=0;
    int keytype=0;
    unsigned char digest[32];
    char fingerprint[65];
    char line[160];
    int result=-1;

    disconnect(a);
    status(a,"Connecting to SSH server...");render(a);
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;addr.sin_port=htons((uint16_t)xssh_form_port(&a->form));
    addr.sin_addr.s_addr=inet_addr(a->form.host);
    if(addr.sin_addr.s_addr==INADDR_NONE){status(a,"Enter a numeric IPv4 server address");goto out;}
    a->sock=socket(AF_INET,SOCK_STREAM,0);
    if(a->sock<0){status(a,"socket() failed");goto out;}
    /* Disable Nagle: Vim and other full-screen apps need single-keystroke
       SSH packets to be sent promptly, even with pending small packets. */
    {int one=1;
     if(setsockopt(a->sock,IPPROTO_TCP,TCP_NODELAY,&one,sizeof(one))!=0)
         status(a,"Warning: TCP_NODELAY unavailable; input may lag");}
    if(connect(a->sock,(struct sockaddr*)&addr,sizeof(addr))<0){status(a,"TCP connection failed");goto out;}
    a->session=libssh2_session_init();
    if(!a->session){status(a,"libssh2_session_init failed");goto out;}
    libssh2_session_set_timeout(a->session,12000);
    status(a,"SSH handshake...");render(a);
    if(libssh2_session_handshake(a->session,a->sock)){
        char *msg=NULL;int len=0;
        libssh2_session_last_error(a->session,&msg,&len,0);
        snprintf(line,sizeof(line),"SSH handshake error: %.*s",len>85?85:len,msg?msg:"");
        status(a,line);goto out;
    }
    hostkey=libssh2_session_hostkey(a->session,&keylen,&keytype);
    if(!hostkey || !keylen || mbedtls_sha256((const unsigned char*)hostkey,keylen,digest,0)!=0){
        status(a,"Could not read SSH host key");goto out;
    }
    for(int i=0;i<32;i++)sprintf(fingerprint+i*2,"%02x",digest[i]);
    fingerprint[64]=0;
    /* The legacy xssh.cfg pin applies only to its configured host and port.
       Manually entered other servers have independent TOFU records. */
    if(a->settings.pin[0] && !strcmp(a->form.host,a->settings.host) &&
       xssh_form_port(&a->form)==a->settings.port) {
        if(!pin_is_valid(a->settings.pin) || !match_pin(a->settings.pin,fingerprint)) {
            status(a,"HOST KEY MISMATCH with xssh.cfg pin. Refusing connection.");goto out;
        }
    } else {
        int known=xssh_known_host_check(KNOWN_HOSTS_FILE,a->form.host,
                                        xssh_form_port(&a->form),fingerprint);
        if(known==2){status(a,"HOST KEY CHANGED! Connection refused.");goto out;}
        if(known<0){status(a,"Could not read trusted SSH host keys");goto out;}
        if(known==0){
            memcpy(a->pending_hostkey,fingerprint,sizeof(fingerprint));
            a->stage=ST_VERIFY_HOST;
            status(a,"Unknown server key: verify SHA-256 fingerprint before trusting");
            return 1; /* Stay connected, but DO NOT send password yet. */
        }
    }
    status(a,"Host key verified; authenticating...");render(a);
    if(ssh_auth_complete(a)==0){result=0;goto out;}
out:
    xssh_form_zero_password(&a->form);
    if(result!=0)disconnect(a);
    return result;
}
static void reject_unknown_host(App *a) {
    disconnect(a);
    xssh_form_zero_password(&a->form);
    memset(a->pending_hostkey,0,sizeof(a->pending_hostkey));
    a->stage=ST_FORM;
    a->form.field=XSSH_FORM_PASSWORD;
    status(a,"Host key not trusted; password was not sent");
}
static void trust_host_and_authenticate(App *a) {
    if(xssh_known_host_add(KNOWN_HOSTS_FILE,a->form.host,
                           xssh_form_port(&a->form),a->pending_hostkey)!=0){
        status(a,"Cannot save trusted host key (writable HDD required)");
        a->stage=ST_FAILED;
        disconnect(a);
    } else if(ssh_auth_complete(a)==0) {
        a->stage=ST_CONNECTED;
    } else {
        a->stage=ST_FAILED;
        disconnect(a);
    }
    xssh_form_zero_password(&a->form);
    memset(a->pending_hostkey,0,sizeof(a->pending_hostkey));
}
static void enqueue(App *a,const unsigned char *data,size_t n) {
    if(a->stage!=ST_CONNECTED||n==0)return;
    if(n>TX_CAP-a->tx_len){status(a,"Outgoing buffer full");return;}
    memcpy(a->outgoing+a->tx_len,data,n);a->tx_len+=n;
}
static void type_byte(App *a,unsigned char c) {
    if(a->stage==ST_FORM)xssh_form_type(&a->form,c);
    else if(a->stage==ST_CONNECTED)enqueue(a,&c,1);
}
static void send_sequence(App*a,const char *s) {
    if(a->stage==ST_CONNECTED)enqueue(a,(const unsigned char*)s,strlen(s));
    else if(a->stage==ST_FORM && s && s[0]==27 && s[1]=='[') {
        if(s[2]=='A' && a->form.field>0)a->form.field--;
        if(s[2]=='B' && a->form.field<XSSH_FORM_PASSWORD)a->form.field++;
        if(s[2]=='Z' && a->form.field>0)a->form.field--;
    }
}
static void backspace(App *a) {
    if(a->stage==ST_VERIFY_HOST){reject_unknown_host(a);return;}
    if(a->stage==ST_FORM)xssh_form_backspace(&a->form);
    else if(a->stage==ST_CONNECTED)type_byte(a,127);
}
static void enter(App*a) {
    if(a->stage==ST_VERIFY_HOST){trust_host_and_authenticate(a);return;}
    if(a->stage==ST_FORM) {
        if(!xssh_form_validate(&a->form,a->form.field)) {
            char message[100];
            snprintf(message,sizeof(message),"Invalid %s; correct it before continuing",
                     xssh_form_field_name(a->form.field));
            status(a,message);return;
        }
        if(a->form.field<XSSH_FORM_PASSWORD){
            a->form.field++;
            status(a,"Enter the next field, then B / Enter");return;
        }
        if(!a->ready){status(a,"Network/seed not ready; check configuration");return;}
        xssh_network_poll(&a->network);
        if(!xssh_network_ready(&a->network)) {
            status(a,"No Xbox IP address yet; check Ethernet/DHCP");return;
        }
        int r=ssh_connect(a);
        if(r==0)a->stage=ST_CONNECTED;
        else if(r<0)a->stage=ST_FAILED;
        /* r==1: first-use host fingerprint displayed for confirmation. */
    } else if(a->stage==ST_FAILED) {
        a->stage=ST_FORM;
        a->form.field=XSSH_FORM_HOST;
        xssh_form_zero_password(&a->form);
        status(a,"Edit connection details; B / Enter advances");
    } else type_byte(a,'\r');
}
static void virtual_key(App *a) {
    if(a->stage==ST_FAILED){enter(a);return;}
    if(a->key_row<4){
        const char *keys=a->shift?keyboard_shift[a->key_row]:keyboard[a->key_row];
        if(a->key_col>=(int)strlen(keys))return;
        type_byte(a,(unsigned char)keys[a->key_col]);
    } else {
        switch(key_index(a)){
        case 0:type_byte(a,' ');break;
        case 1:enter(a);break;
        case 2:backspace(a);break;
        case 3:if(a->stage==ST_FORM)enter(a);else type_byte(a,'\t');break;
        case 4:if(a->stage==ST_VERIFY_HOST)reject_unknown_host(a);else type_byte(a,27);break;
        case 5:type_byte(a,3);break;
        }
    }
}
/* Never let a busy program's stdout starve interactive keyboard input.
   This function is always called before the read burst and once afterward.
   It never blocks: libssh2's session and socket are both nonblocking. */
static void ssh_flush_outgoing(App *a) {
    if(a->stage!=ST_CONNECTED || !a->channel || !a->tx_len)return;
    ssize_t n=libssh2_channel_write(a->channel,(const char*)a->outgoing,a->tx_len);
    if(n>0){
        a->tx_len-=(size_t)n;
        if(a->tx_len)memmove(a->outgoing,a->outgoing+n,a->tx_len);
    } else if(n!=LIBSSH2_ERROR_EAGAIN && n!=0){
        a->stage=ST_FAILED;
        status(a,"SSH write error");
        disconnect(a);
    }
}
static void ssh_pump(App *a) {
    if(a->stage!=ST_CONNECTED || !a->channel)return;
    /* Interactive SSH input gets the FIRST opportunity to use the socket. */
    ssh_flush_outgoing(a);
    if(a->stage!=ST_CONNECTED)return;

    unsigned char data[4096];
    const unsigned int begin=SDL_GetTicks();
    for(int j=0;j<SSH_READ_BURST_MAX;j++) {
        if((unsigned)(SDL_GetTicks()-begin)>=SSH_READ_TIME_BUDGET_MS)break;
        ssize_t n=libssh2_channel_read(a->channel,(char*)data,sizeof(data));
        if(n>0){
            term_feed(&a->term,data,(unsigned int)n);
            unsigned char reply[128];
            unsigned int r=term_take_response(&a->term,reply,sizeof(reply));
            if(r)enqueue(a,reply,r);
        }
        else if(n==LIBSSH2_ERROR_EAGAIN || n==0)break;
        else {a->stage=ST_FAILED;status(a,"SSH read error");disconnect(a);return;}
        /* Device input and redraw must get CPU time on an original Xbox. */
    }
    /* Answers to terminal status queries can be sent in the same frame. */
    ssh_flush_outgoing(a);
    if(a->stage!=ST_CONNECTED)return;
    if(libssh2_channel_eof(a->channel)){
        a->stage=ST_FAILED;status(a,"Remote shell closed");disconnect(a);
    }
}
static void nav(App *a,int dx,int dy) {
    a->key_row=(a->key_row+dy+5)%5;
    if(dx) a->key_col=(a->key_col+dx+14)%14;
    int limit=(a->key_row==0)?12:(a->key_row==1)?13:
              (a->key_row==2)?11:(a->key_row==3)?10:14;
    if(a->key_col>=limit)a->key_col=limit-1;
}
static void keyboard_event(App*a,const SDL_KeyboardEvent *e) {
    if(e->repeat)return;
    if((e->keysym.mod & KMOD_CTRL) &&
       (e->keysym.sym==SDLK_EQUALS || e->keysym.sym==SDLK_PLUS ||
        e->keysym.sym==SDLK_KP_PLUS)){
        change_font_size(a,1);return;
    }
    if((e->keysym.mod & KMOD_CTRL) &&
       (e->keysym.sym==SDLK_MINUS || e->keysym.sym==SDLK_KP_MINUS)){
        change_font_size(a,-1);return;
    }
    if((e->keysym.mod & KMOD_CTRL) && e->keysym.sym==SDLK_f){change_font_style(a);return;}
    if((e->keysym.mod & KMOD_CTRL) && e->keysym.sym==SDLK_a){toggle_text_antialiasing(a);return;}
    if(a->stage==ST_POWER_CONFIRM){
        if(e->keysym.sym==SDLK_y || e->keysym.sym==SDLK_RETURN || e->keysym.sym==SDLK_KP_ENTER)confirm_power(a,1);
        else if(e->keysym.sym==SDLK_n || e->keysym.sym==SDLK_ESCAPE)confirm_power(a,0);
        return;
    }
    if(a->stage==ST_FORM){
        if(e->keysym.sym==SDLK_F9){startup_action(a,1);return;}
        if(e->keysym.sym==SDLK_F10){startup_action(a,2);return;}
        if(e->keysym.sym==SDLK_F11){startup_action(a,3);return;}
        if(e->keysym.sym==SDLK_ESCAPE){a->menu_focus=!a->menu_focus;return;}
        if(e->keysym.sym==SDLK_TAB){a->menu_focus=!a->menu_focus;return;}
        if(a->menu_focus){
            if(e->keysym.sym==SDLK_LEFT){a->menu_action=(a->menu_action+3)%4;return;}
            if(e->keysym.sym==SDLK_RIGHT){a->menu_action=(a->menu_action+1)%4;return;}
            if(e->keysym.sym==SDLK_RETURN || e->keysym.sym==SDLK_KP_ENTER){startup_action(a,a->menu_action);return;}
        }
    }
    switch(e->keysym.sym){
    case SDLK_RETURN:case SDLK_KP_ENTER:enter(a);break;
    case SDLK_BACKSPACE:backspace(a);break;
    case SDLK_TAB:if(a->stage==ST_FORM){
        if(e->keysym.mod & KMOD_SHIFT){if(a->form.field>0)a->form.field--;}
        else enter(a);
    }else type_byte(a,9);break;
    case SDLK_ESCAPE:if(a->stage==ST_VERIFY_HOST)reject_unknown_host(a);else type_byte(a,27);break;
    case SDLK_UP:send_sequence(a,"\033[A");break;
    case SDLK_DOWN:send_sequence(a,"\033[B");break;
    case SDLK_RIGHT:send_sequence(a,"\033[C");break;
    case SDLK_LEFT:send_sequence(a,"\033[D");break;
    case SDLK_c: if(e->keysym.mod & KMOD_CTRL)type_byte(a,3);break;
    default:break;
    }
}
/* HID reports are decoded on the main thread, NEVER in the USB IRQ callback.
   Password mode accepts printable ASCII only; connected mode forwards control
   sequences, including Ctrl+C, Ctrl+D, arrows, Home/End and function keys. */
static void usb_key_event(void *ctx,XsshKeyEvent event) {
    App *a=(App*)ctx;
    a->last_input_ms=SDL_GetTicks();
    if(a->saver_active){saver_stop(a);return;}
    if(event.kind==XSSH_KEY_FONT_CYCLE){change_font_style(a);return;}
    if(event.kind==XSSH_KEY_TOGGLE_ANTIALIASING){toggle_text_antialiasing(a);return;}
    if(a->stage==ST_POWER_CONFIRM){
        if(event.kind==XSSH_KEY_ENTER || (event.kind==XSSH_KEY_TEXT && (event.byte=='y'||event.byte=='Y')))confirm_power(a,1);
        else if(event.kind==XSSH_KEY_TEXT && (event.byte=='n'||event.byte=='N'||event.byte==27))confirm_power(a,0);
        return;
    }
    if(a->stage==ST_FORM){
        if(event.kind==XSSH_KEY_TEXT && event.byte==27){a->menu_focus=!a->menu_focus;return;}
        if(event.kind==XSSH_KEY_TEXT && event.byte==9){a->menu_focus=!a->menu_focus;return;}
        if(event.kind==XSSH_KEY_SEQUENCE && event.sequence){
            if(!strcmp(event.sequence,"\033[20~")){startup_action(a,1);return;}
            if(!strcmp(event.sequence,"\033[21~")){startup_action(a,2);return;}
            if(!strcmp(event.sequence,"\033[23~")){startup_action(a,3);return;}
        }
        if(a->menu_focus){
            if(event.kind==XSSH_KEY_ENTER){startup_action(a,a->menu_action);return;}
            if(event.kind==XSSH_KEY_SEQUENCE && event.sequence){
                if(!strcmp(event.sequence,"\033[D"))a->menu_action=(a->menu_action+3)%4;
                else if(!strcmp(event.sequence,"\033[C"))a->menu_action=(a->menu_action+1)%4;
                return;
            }
        }
    }
    switch(event.kind) {
    case XSSH_KEY_TEXT:
        if(event.byte==27 && a->stage==ST_VERIFY_HOST)reject_unknown_host(a);
        else if(event.byte==9 && a->stage==ST_FORM)enter(a);
        else type_byte(a,event.byte);
        break;
    case XSSH_KEY_FONT_INCREASE: change_font_size(a,1);break;
    case XSSH_KEY_FONT_DECREASE: change_font_size(a,-1);break;
    case XSSH_KEY_FONT_CYCLE: change_font_style(a);break;
    case XSSH_KEY_TOGGLE_ANTIALIASING: toggle_text_antialiasing(a);break;
    case XSSH_KEY_ENTER: enter(a);break;
    case XSSH_KEY_BACKSPACE: backspace(a);break;
    case XSSH_KEY_SEQUENCE: if(event.sequence)send_sequence(a,event.sequence);break;
    }
}
static void poll_pad(App*a) {
    if(!a->pad && SDL_NumJoysticks()>0 && SDL_IsGameController(0))a->pad=SDL_GameControllerOpen(0);
    if(!a->pad)return;
    const SDL_GameControllerButton buttons[]={
        SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B,
        SDL_CONTROLLER_BUTTON_X,SDL_CONTROLLER_BUTTON_Y,
        SDL_CONTROLLER_BUTTON_START,SDL_CONTROLLER_BUTTON_BACK,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
        SDL_CONTROLLER_BUTTON_DPAD_UP,SDL_CONTROLLER_BUTTON_DPAD_DOWN
    };
    unsigned int now=0;
    for(int i=0;i<10;i++)if(SDL_GameControllerGetButton(a->pad,buttons[i]))now |= (1u<<i);
    unsigned int pressed=now & ~a->pad_old;
    a->pad_old=now;
    if(pressed){
        a->last_input_ms=SDL_GetTicks();
        if(a->saver_active){saver_stop(a);return;}
    }
    /* Do not activate invisible virtual keys while a USB keyboard is present.
       Controller Enter/Backspace/Ctrl-C/disconnect still work. */
    int software_keyboard=xssh_software_keyboard_visible();
    if(a->stage==ST_POWER_CONFIRM){
        if(pressed&(1u<<1))confirm_power(a,1);
        if(pressed&(1u<<2))confirm_power(a,0);
        return;
    }
    if(a->stage==ST_FORM && a->menu_focus){
        if(pressed&(1u<<0) || pressed&(1u<<1))startup_action(a,a->menu_action);
        if(pressed&(1u<<6))a->menu_action=(a->menu_action+3)%4;
        if(pressed&(1u<<7))a->menu_action=(a->menu_action+1)%4;
        if(pressed&(1u<<4) || pressed&(1u<<5))a->menu_focus=0;
        return;
    }
    if(software_keyboard && (pressed&(1u<<0)))virtual_key(a);
    if(pressed&(1u<<1))enter(a);
    if(pressed&(1u<<2))backspace(a);
    if(software_keyboard && (pressed&(1u<<3)))a->shift=!a->shift;
    if(pressed&(1u<<4)){
        if(a->stage==ST_FORM && a->form.field>0)a->form.field--;
        else type_byte(a,3);
    }
    if(pressed&(1u<<5)){
        if(a->stage==ST_CONNECTED){
            disconnect(a);a->stage=ST_FORM;
            xssh_form_zero_password(&a->form);a->form.field=XSSH_FORM_HOST;
            status(a,"Disconnected; enter a new server or reconnect");
        } else if(a->stage==ST_VERIFY_HOST)reject_unknown_host(a);
        else if(a->stage==ST_FORM)a->menu_focus=1;
    }
    if(software_keyboard && (pressed&(1u<<6)))nav(a,-1,0);
    if(software_keyboard && (pressed&(1u<<7)))nav(a,1,0);
    if(software_keyboard && (pressed&(1u<<8)))nav(a,0,-1);
    if(software_keyboard && (pressed&(1u<<9)))nav(a,0,1);
}
int main(void) {
    App *a=(App*)calloc(1,sizeof(App));
    if(!a)return 1;
    a->sock=-1;a->running=1;
    a->last_input_ms=SDL_GetTicks();
    term_init(&a->term);
    /* Read configuration before initializing video. This also leaves the safe
       auto default in place when config is absent or SSH fields are invalid. */
    int settings_result = load_settings(&a->settings);
    int settings_ok = settings_result == 0;
    int cols=TERM_COLS, rows=TERM_ROWS;
    a->term_height=PAD_TERMINAL_HEIGHT;
    xssh_font_layout_for_height(a->settings.font_size,a->term_height,
                     &a->terminal_glyph_w,&a->terminal_glyph_h,&cols,&rows);
    if(!term_set_size(&a->term,cols,rows))return 1;
    if(!xssh_video_initialize(a->settings.video_mode,&a->video))return 1;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)<0)return 1;
    a->last_input_ms=SDL_GetTicks();
    a->window=SDL_CreateWindow("XSSH",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,
                               a->video.width,a->video.height,SDL_WINDOW_SHOWN);
    if(!a->window)return 1;
    a->renderer=SDL_CreateRenderer(a->window,-1,0);
    if(!a->renderer)return 1;
    /* Keep the logical 640x480 UI at 480i through 1080i and custom modes.
       The terminal font size changes the grid, not the physical video mode. */
    if(SDL_RenderSetLogicalSize(a->renderer,WIDTH,HEIGHT)!=0)return 1;
    /* Nearest-neighbor sampling must be set before creating font textures. */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
    if(init_font(a))return 1;
    a->term_cache=SDL_CreateTexture(a->renderer,SDL_PIXELFORMAT_RGBA8888,
                                    SDL_TEXTUREACCESS_TARGET,WIDTH,USB_TERMINAL_HEIGHT);
    if(a->term_cache)SDL_SetTextureBlendMode(a->term_cache,SDL_BLENDMODE_NONE);
    /* Always show an editable login form, even if configuration, network,
       or seed initialization fails. The status bar explains the blocker. */
    a->stage=ST_FORM;
    logline(a,"XSSH: original Xbox SSH2 terminal / interactive login v3");
    logline(a,XSSH_BUILD_ID);
    {char info[100];
     snprintf(info,sizeof(info),"Video: %s; font: %d; terminal: %dx%d",
              a->video.description,a->settings.font_size,a->term.cols,a->term.rows);
     logline(a,info);
    }
    if(a->video.warning[0])logline(a,a->video.warning);
    xssh_form_init(&a->form,a->settings.host,a->settings.port,a->settings.user);
    if(!settings_ok){
        status(a,settings_result == -5 ?
               "Invalid LED or screensaver setting in D:\\xssh.cfg" :
               settings_result == -4 ?
               "Invalid font, style or text_antialiasing in D:\\xssh.cfg" :
               settings_result == -3 ?
               "Invalid theme: use xbox or inverted_xbox in D:\\xssh.cfg" :
               settings_result == -2 ?
               "Invalid network settings in D:\\xssh.cfg" :
               "Missing/invalid D:\\xssh.cfg; deploy it beside default.xbe");
    } else {
        char error[120];
        if(xssh_network_initialize(&a->settings.network,&a->network,error,sizeof(error))!=0)
            status(a,error[0]?error:"Network initialization failed");
        else {
            char line[120];
            snprintf(line,sizeof(line),"Network: %s, Xbox IP: %s",
                     xssh_network_mode_name(a->network.mode),a->network.address);
            logline(a,line);
            if(xssh_rng_prepare()!=0)
                status(a,"Cannot read/update D:\\xssh-seed.bin (64 bytes; writable HDD required)");
            else if(libssh2_init(0)!=0)
                status(a,"libssh2_init failed");
            else {
                a->ready=1; a->stage=ST_FORM;
                status(a,"Enter server IP, port, username, password; B/Enter advances");
            }
        }
    }
    /* SDL's Xbox controller backend initialized usbh_core_init() above.
       Register the HID class before the first USB hub poll: the Makefile
       disables SDL's 500ms early joystick enumeration for this reason. */
    xssh_usb_keyboard_init();
    SDL_StartTextInput();
    while(a->running){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_TEXTINPUT && a->swallow_wake_text){
                a->swallow_wake_text=0;
                continue;
            }
            if(ev.type==SDL_KEYDOWN || ev.type==SDL_TEXTINPUT || ev.type==SDL_CONTROLLERBUTTONDOWN){
                a->last_input_ms=SDL_GetTicks();
                if(a->saver_active){
                    a->swallow_wake_text=(ev.type==SDL_KEYDOWN);
                    saver_stop(a);continue;
                }
                if(ev.type==SDL_KEYDOWN)a->swallow_wake_text=0;
            }
            if(ev.type==SDL_QUIT)a->running=0;
            else if(ev.type==SDL_KEYDOWN)keyboard_event(a,&ev.key);
            else if(ev.type==SDL_TEXTINPUT && (a->stage==ST_FORM || a->stage==ST_CONNECTED)){
                for(const unsigned char *p=(const unsigned char*)ev.text.text;*p;p++)
                    if(*p>=32&&*p<127 && !(a->stage==ST_FORM && a->menu_focus))type_byte(a,*p);
            }
        }
        {unsigned int now=SDL_GetTicks();
         if(now-a->last_network_poll>=500){
             a->last_network_poll=now;
             xssh_network_poll(&a->network);
         }
         xssh_usb_keyboard_poll(now,usb_key_event,a);}
        poll_pad(a);
        resize_for_keyboard(a,0);
        ssh_pump(a);
        {unsigned int now=SDL_GetTicks();
         if(!a->saver_active &&
            (unsigned int)(now-a->last_input_ms)>=(unsigned int)a->settings.screensaver_seconds*1000u)
             saver_start(a,now);
         update_front_led(a);
         /* Skip unnecessary full-screen redraws while the terminal is idle.
            Keystrokes and incoming SSH text still trigger an immediate redraw
            on the next ~30 FPS boundary. */
         if(now-a->last_render_ms>=33){
             if(a->saver_active){saver_draw(a,now);a->last_render_ms=now;}
             else if(a->stage!=ST_CONNECTED || a->term.dirty || !a->cache_valid){
                 render(a);a->last_render_ms=now;
             }
         }
        }
        SDL_Delay(2);
    }
    saver_stop(a);
    xssh_led_restore();
    disconnect(a);
    libssh2_exit();
    if(a->pad)SDL_GameControllerClose(a->pad);
    if(a->term_cache)SDL_DestroyTexture(a->term_cache);
    if(a->font)SDL_DestroyTexture(a->font);
    if(a->font_aa)SDL_DestroyTexture(a->font_aa);
    SDL_DestroyRenderer(a->renderer);SDL_DestroyWindow(a->window);
    SDL_Quit();free(a);
    return 0;
}
