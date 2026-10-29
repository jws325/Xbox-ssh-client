/* XSSH - an SSH2 terminal client for the original Xbox / nxdk.
   Networking: nxdk lwIP; crypto: libssh2 + Mbed TLS.
   Host-key pinning is mandatory; password is entered at runtime only. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <SDL.h>
#include <hal/video.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <libssh2.h>
#include <mbedtls/sha256.h>
#include "font8x12.h"
#include "terminal.h"
#include "rng.h"
#include "usb_keyboard.h"
#include "video_xbox.h"
#include "network_config.h"
#include "network_xbox.h"
#include "theme.h"
#include "font_size.h"

#define WIDTH 640
#define HEIGHT 480
#define GLYPH_W 8
#define GLYPH_H 12
#define SCREEN_Y 30
#define KEY_Y 342
#define KEY_W 44
#define KEY_H 23
#define PASS_CAP 128
#define TX_CAP 2048
#define CONFIG_FILE "D:\\xssh.cfg"

typedef enum { ST_PASSWORD, ST_CONNECTED, ST_FAILED } Stage;
typedef struct {
    char host[80];
    int port;
    char user[80];
    char pin[65];
    char video_mode[48];
    XsshTheme theme;
    int theme_config_valid;
    int font_size;
    int font_config_valid;
    XsshNetworkConfig network;
    int network_config_valid;
} Settings;
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *font;
    SDL_GameController *pad;
    Terminal term;
    int terminal_glyph_w, terminal_glyph_h;
    Settings settings;
    XsshVideoActive video;
    XsshNetworkState network;
    unsigned int last_network_poll;
    Stage stage;
    char pass[PASS_CAP];
    size_t pass_len;
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
} App;

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
    s->font_config_valid=1;
    xssh_network_default(&s->network);
    s->network_config_valid=1;
    snprintf(s->video_mode,sizeof(s->video_mode),"auto");
    FILE *f=fopen(CONFIG_FILE,"rb");
    if(!f)return -1;
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
    }
    fclose(f);
    if(!s->theme_config_valid)return -3;
    if(!s->font_config_valid)return -4;
    if(!s->host[0]||!s->user[0]||s->port<1||s->port>65535)return -1;
    /* Do not silently use DHCP if the user explicitly configured invalid
       static settings; that could put credentials on an unintended network. */
    if(!s->network_config_valid || !xssh_network_validate(&s->network,NULL,0))return -2;
    return 0;
}
static int init_font(App *a) {
    SDL_Surface *s=SDL_CreateRGBSurface(0,16*8,6*12,32,
                                       0x00ff0000,0x0000ff00,0x000000ff,0xff000000);
    if(!s)return -1;
    uint32_t white=SDL_MapRGBA(s->format,255,255,255,255);
    uint32_t blank=SDL_MapRGBA(s->format,255,255,255,0);
    if(SDL_MUSTLOCK(s))SDL_LockSurface(s);
    for(int y=0;y<s->h;y++){
        uint32_t *dst=(uint32_t*)((uint8_t*)s->pixels+y*s->pitch);
        for(int x=0;x<s->w;x++){
            int glyph=(y/12)*16+x/8;
            uint8_t bit=(glyph<95)?xssh_font[glyph][y%12]:0;
            dst[x]=(bit & (0x80u>>(x%8)))?white:blank;
        }
    }
    if(SDL_MUSTLOCK(s))SDL_UnlockSurface(s);
    a->font=SDL_CreateTextureFromSurface(a->renderer,s);
    SDL_FreeSurface(s);
    if(!a->font)return -1;
    SDL_SetTextureBlendMode(a->font,SDL_BLENDMODE_BLEND);
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
    SDL_Rect src={(n%16)*8,(n/16)*12,8,12};
    SDL_Rect dst={x,y,w,h};
    SDL_SetTextureColorMod(a->font,color.r,color.g,color.b);
    SDL_RenderCopy(a->renderer,a->font,&src,&dst);
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
static void render(App *a) {
    XsshThemeColors c=xssh_theme_colors(a->settings.theme);
    SDL_SetRenderDrawColor(a->renderer,c.background.r,c.background.g,c.background.b,255);
    SDL_RenderClear(a->renderer);
    text_at(a,8,6,"XSSH / ORIGINAL XBOX",c.foreground,25);
    text_at(a,290,6,a->video.description,c.foreground,22);
    const char *lab=(a->stage==ST_CONNECTED)?"SSH CONNECTED":
                    (a->stage==ST_PASSWORD)?"PASSWORD ENTRY":"DISCONNECTED";
    text_at(a,496,6,lab,c.foreground,18);
    /* Header/terminal/status areas retain the theme's background. */
    rect(a,0,23,WIDTH,1,c.foreground);
    rect(a,0,SCREEN_Y-1,WIDTH,a->term.rows*a->terminal_glyph_h+2,c.background);
    for(int y=0;y<a->term.rows;y++)for(int x=0;x<a->term.cols;x++) {
        unsigned char ch=a->term.cells[y][x];
        if(ch==' ')continue;
        XsshRgb color=xssh_theme_terminal_color(a->settings.theme,a->term.fg[y][x]&15);
        glyph_scaled(a,x*a->terminal_glyph_w,SCREEN_Y+y*a->terminal_glyph_h,
                     (char)ch,color,a->terminal_glyph_w,a->terminal_glyph_h);
    }
    if(a->stage==ST_CONNECTED&&a->term.cursor_visible) {
        int cursor_h=a->terminal_glyph_h>=12?2:1;
        rect(a,a->term.x*a->terminal_glyph_w,
             SCREEN_Y+(a->term.y+1)*a->terminal_glyph_h-cursor_h,
             a->terminal_glyph_w,cursor_h,c.foreground);
    }
    rect(a,0,326,WIDTH,1,c.foreground);
    if(a->stage==ST_PASSWORD){
        char prompt[100];
        snprintf(prompt,sizeof(prompt),"PASSWORD: %u characters - B/ENTER to connect",(unsigned)a->pass_len);
        text_at(a,6,329,prompt,c.foreground,76);
    } else text_at(a,4,329,a->status,c.foreground,78);
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
    } else {
        text_at(a,12,353,"USB KEYBOARD CONNECTED",c.foreground,52);
        text_at(a,12,379,"Type using your USB keyboard.",c.foreground,55);
        text_at(a,12,402,"Unplug it to restore the controller keyboard.",c.foreground,55);
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
    status(a,"Connecting... (IPv4 only)");render(a);
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;addr.sin_port=htons((uint16_t)a->settings.port);
    addr.sin_addr.s_addr=inet_addr(a->settings.host);
    if(addr.sin_addr.s_addr==INADDR_NONE){status(a,"Config must contain numeric IPv4 host.");goto out;}
    a->sock=socket(AF_INET,SOCK_STREAM,0);
    if(a->sock<0){status(a,"socket() failed");goto out;}
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
    logline(a,"Negotiated SSH host key fingerprint (SHA256 hex):");
    logline(a,fingerprint);
    if(!pin_is_valid(a->settings.pin)){
        status(a,"Host key unpinned. Set hostkey_sha256 in D:\\xssh.cfg");goto out;
    }
    if(!match_pin(a->settings.pin,fingerprint)){
        status(a,"HOST KEY MISMATCH! Connection refused.");goto out;
    }
    status(a,"Host key verified; authenticating...");render(a);
    if(libssh2_userauth_password(a->session,a->settings.user,a->pass)){
        status(a,"Password authentication failed");goto out;
    }
    a->channel=libssh2_channel_open_session(a->session);
    if(!a->channel){status(a,"Could not open SSH channel");goto out;}
    if(libssh2_channel_request_pty_ex(a->channel,"xterm",5,NULL,0,
                                       a->term.cols,a->term.rows,0,0)!=0){
        status(a,"PTY request failed");goto out;
    }
    if(libssh2_channel_shell(a->channel)!=0){status(a,"Shell request failed");goto out;}
    {unsigned long nonblocking=1;
     if(ioctlsocket(a->sock,FIONBIO,&nonblocking)!=0){status(a,"Cannot enable nonblocking TCP socket");goto out;}}
    libssh2_session_set_blocking(a->session,0);
    status(a,"Connected - remote shell ready");
    result=0;
out:
    /* Password only remains in memory until the auth attempt. */
    {volatile char *p=a->pass;for(size_t i=0;i<sizeof(a->pass);i++)p[i]=0;}
    a->pass_len=0;
    if(result!=0)disconnect(a);
    return result;
}
static void enqueue(App *a,const unsigned char *data,size_t n) {
    if(a->stage!=ST_CONNECTED||n==0)return;
    if(n>TX_CAP-a->tx_len){status(a,"Outgoing buffer full");return;}
    memcpy(a->outgoing+a->tx_len,data,n);a->tx_len+=n;
}
static void type_byte(App *a,unsigned char c) {
    if(a->stage==ST_PASSWORD){
        if(c>=32 && c<=126 && a->pass_len+1<PASS_CAP){a->pass[a->pass_len++]=(char)c;a->pass[a->pass_len]=0;}
    }else if(a->stage==ST_CONNECTED)enqueue(a,&c,1);
}
static void send_sequence(App*a,const char *s) {enqueue(a,(const unsigned char*)s,strlen(s));}
static void backspace(App *a) {
    if(a->stage==ST_PASSWORD){
        if(a->pass_len){a->pass[--a->pass_len]=0;}
    }else type_byte(a,127);
}
static void enter(App*a) {
    if(a->stage==ST_PASSWORD){
        if(!a->pass_len){status(a,"Enter the SSH password first");return;}
        xssh_network_poll(&a->network);
        if(!xssh_network_ready(&a->network)){
            status(a,"No Xbox IP address yet; check Ethernet/DHCP settings");return;
        }
        a->stage=(ssh_connect(a)==0)?ST_CONNECTED:ST_FAILED;
    } else if(a->stage==ST_FAILED){
        if(!a->ready){status(a,"Fix config/seed and restart the app");return;}
        a->stage=ST_PASSWORD;
        term_set_size(&a->term,a->term.cols,a->term.rows);
        status(a,"Enter password and press B to retry");
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
        case 3:type_byte(a,'\t');break;
        case 4:type_byte(a,27);break;
        case 5:type_byte(a,3);break;
        }
    }
}
static void ssh_pump(App *a) {
    if(a->stage!=ST_CONNECTED || !a->channel)return;
    unsigned char data[1024];
    for(int j=0;j<8;j++) {
        ssize_t n=libssh2_channel_read(a->channel,(char*)data,sizeof(data));
        if(n>0)term_feed(&a->term,data,(unsigned int)n);
        else if(n==LIBSSH2_ERROR_EAGAIN)break;
        else if(n==0)break;
        else {a->stage=ST_FAILED;status(a,"SSH read error");disconnect(a);return;}
    }
    if(a->tx_len){
        ssize_t n=libssh2_channel_write(a->channel,(const char*)a->outgoing,a->tx_len);
        if(n>0){a->tx_len-=(size_t)n;memmove(a->outgoing,a->outgoing+n,a->tx_len);}
        else if(n!=LIBSSH2_ERROR_EAGAIN && n!=0){
            a->stage=ST_FAILED;status(a,"SSH write error");disconnect(a);return;
        }
    }
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
    switch(e->keysym.sym){
    case SDLK_RETURN:case SDLK_KP_ENTER:enter(a);break;
    case SDLK_BACKSPACE:backspace(a);break;
    case SDLK_TAB:type_byte(a,9);break;
    case SDLK_ESCAPE:type_byte(a,27);break;
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
    switch(event.kind) {
    case XSSH_KEY_TEXT: type_byte(a,event.byte);break;
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
    /* Do not activate invisible virtual keys while a USB keyboard is present.
       Controller Enter/Backspace/Ctrl-C/disconnect still work. */
    int software_keyboard=xssh_software_keyboard_visible();
    if(software_keyboard && (pressed&(1u<<0)))virtual_key(a);
    if(pressed&(1u<<1))enter(a);
    if(pressed&(1u<<2))backspace(a);
    if(software_keyboard && (pressed&(1u<<3)))a->shift=!a->shift;
    if(pressed&(1u<<4))type_byte(a,3);
    if(pressed&(1u<<5)){
        if(a->stage==ST_CONNECTED){a->stage=ST_FAILED;disconnect(a);status(a,"Disconnected by user");}
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
    term_init(&a->term);
    /* Read configuration before initializing video. This also leaves the safe
       auto default in place when config is absent or SSH fields are invalid. */
    int settings_result = load_settings(&a->settings);
    int settings_ok = settings_result == 0;
    int cols=TERM_COLS, rows=TERM_ROWS;
    xssh_font_layout(a->settings.font_size,
                     &a->terminal_glyph_w,&a->terminal_glyph_h,&cols,&rows);
    if(!term_set_size(&a->term,cols,rows))return 1;
    if(!xssh_video_initialize(a->settings.video_mode,&a->video))return 1;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)<0)return 1;
    a->window=SDL_CreateWindow("XSSH",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,
                               a->video.width,a->video.height,SDL_WINDOW_SHOWN);
    if(!a->window)return 1;
    a->renderer=SDL_CreateRenderer(a->window,-1,0);
    if(!a->renderer)return 1;
    /* Keep the logical 640x480 UI at 480i through 1080i and custom modes.
       The terminal font size changes the grid, not the physical video mode. */
    if(SDL_RenderSetLogicalSize(a->renderer,WIDTH,HEIGHT)!=0)return 1;
    /* Avoid blurry filtering when scaling the bitmap terminal glyphs. */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
    if(init_font(a))return 1;
    a->stage=ST_FAILED;
    logline(a,"XSSH: original Xbox SSH2 terminal");
    {char info[100];
     snprintf(info,sizeof(info),"Video: %s; font: %d; terminal: %dx%d",
              a->video.description,a->settings.font_size,a->term.cols,a->term.rows);
     logline(a,info);
    }
    if(a->video.warning[0])logline(a,a->video.warning);
    if(!settings_ok){
        status(a,settings_result == -4 ?
               "Invalid font_size: choose 8 through 24 in D:\\xssh.cfg" :
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
                a->ready=1; a->stage=ST_PASSWORD;
                status(a,"Enter SSH password; B/Enter to connect (wait for IP)");
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
            if(ev.type==SDL_QUIT)a->running=0;
            else if(ev.type==SDL_KEYDOWN)keyboard_event(a,&ev.key);
            else if(ev.type==SDL_TEXTINPUT && a->stage!=ST_FAILED){
                for(const unsigned char *p=(const unsigned char*)ev.text.text;*p;p++)
                    if(*p>=32&&*p<127)type_byte(a,*p);
            }
        }
        {unsigned int now=SDL_GetTicks();
         if(now-a->last_network_poll>=500){
             a->last_network_poll=now;
             xssh_network_poll(&a->network);
         }
         xssh_usb_keyboard_poll(now,usb_key_event,a);}
        poll_pad(a);
        ssh_pump(a);
        render(a);
        SDL_Delay(16);
    }
    disconnect(a);
    libssh2_exit();
    if(a->pad)SDL_GameControllerClose(a->pad);
    if(a->font)SDL_DestroyTexture(a->font);
    SDL_DestroyRenderer(a->renderer);SDL_DestroyWindow(a->window);
    SDL_Quit();free(a);
    return 0;
}
