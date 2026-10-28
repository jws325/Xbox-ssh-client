/* USB HID Boot Keyboard -> US ANSI terminal key/character translation.
   Portable logic kept separate from the nxdk OHCI driver for native tests. */
#include "hid_keys.h"
#include <string.h>

#define MOD_CTRL  0x11u /* left/right Ctrl */
#define MOD_SHIFT 0x22u /* left/right Shift */
#define MOD_ALT   0x44u /* left/right Alt */
#define REPEAT_DELAY 440u
#define REPEAT_INTERVAL 45u

static int in_report(const uint8_t r[8], uint8_t key) {
    for(int i=2;i<8;i++) if(r[i]==key)return 1;
    return 0;
}

static void emit_byte(XsshKeySink sink,void *ctx,unsigned char byte) {
    XsshKeyEvent e={XSSH_KEY_TEXT,byte,NULL};sink(ctx,e);
}
static void emit_seq(XsshKeySink sink,void *ctx,const char *seq) {
    XsshKeyEvent e={XSSH_KEY_SEQUENCE,0,seq};sink(ctx,e);
}
static void emit_special(XsshKeySink sink,void *ctx,XsshKeyKind kind) {
    XsshKeyEvent e={kind,0,NULL};sink(ctx,e);
}

/* Standard US boot keyboard: printable keys, no locale/IME translation. */
static unsigned char printable(uint8_t key,int shift,int caps) {
    static const char digits[]="1234567890";
    static const char shifted_digits[]="!@#$%^&*()";
    static const char punct[]="-=[]\\#;'`,./";
    static const char punct_shift[]="_+{}|~:\"~<>?";
    if(key>=0x04 && key<=0x1d) {
        unsigned char ch=(unsigned char)('a'+key-0x04);
        return (shift ^ caps)?(unsigned char)(ch-'a'+'A'):ch;
    }
    if(key>=0x1e && key<=0x27)
        return (unsigned char)(shift?shifted_digits[key-0x1e]:digits[key-0x1e]);
    if(key==0x2c)return ' ';
    if(key>=0x2d && key<=0x38)
        return (unsigned char)(shift?punct_shift[key-0x2d]:punct[key-0x2d]);
    if(key==0x64)return shift?'|':'\\'; /* non-US ISO key, US approximation */
    if(key>=0x54 && key<=0x57) {
        static const char ops[]="/*-+";
        return (unsigned char)ops[key-0x54];
    }
    if(key>=0x59 && key<=0x61) { /* numeric keypad 1..9 */
        static const char pad[]="123456789";
        return (unsigned char)pad[key-0x59];
    }
    if(key==0x62)return '0';
    if(key==0x63)return '.';
    if(key==0x67)return '=';
    return 0;
}

static const char *sequence_for(uint8_t key,int shift) {
    switch(key) {
        case 0x2b: return shift?"\033[Z":NULL; /* Shift-Tab */
        case 0x3a: return "\033OP"; /* F1 */
        case 0x3b: return "\033OQ";
        case 0x3c: return "\033OR";
        case 0x3d: return "\033OS";
        case 0x3e: return "\033[15~";
        case 0x3f: return "\033[17~";
        case 0x40: return "\033[18~";
        case 0x41: return "\033[19~";
        case 0x42: return "\033[20~";
        case 0x43: return "\033[21~";
        case 0x44: return "\033[23~";
        case 0x45: return "\033[24~";
        case 0x49: return "\033[2~"; /* Insert */
        case 0x4a: return "\033[H";  /* Home */
        case 0x4b: return "\033[5~"; /* Page Up */
        case 0x4c: return "\033[3~"; /* Delete */
        case 0x4d: return "\033[F";  /* End */
        case 0x4e: return "\033[6~"; /* Page Down */
        case 0x4f: return "\033[C";  /* Right */
        case 0x50: return "\033[D";  /* Left */
        case 0x51: return "\033[B";  /* Down */
        case 0x52: return "\033[A";  /* Up */
        default: return NULL;
    }
}

static void send_key(XsshHidKeys *s,uint8_t key,uint8_t mods,
                     XsshKeySink sink,void *ctx) {
    int shift=(mods & MOD_SHIFT)!=0;
    int ctrl=(mods & MOD_CTRL)!=0;
    int alt=(mods & MOD_ALT)!=0;
    unsigned char ch;
    const char *seq;

    if(key==0x39) {s->caps_lock^=1;return;}
    if(key==0x53) return; /* NumLock: keypad is always numeric in this port */
    if(key==0x28 || key==0x58) {emit_special(sink,ctx,XSSH_KEY_ENTER);return;}
    if(key==0x2a) {emit_special(sink,ctx,XSSH_KEY_BACKSPACE);return;}
    if(key==0x29) {emit_byte(sink,ctx,27);return;}
    seq=sequence_for(key,shift);
    if(seq) {emit_seq(sink,ctx,seq);return;}
    if(key==0x2b) {emit_byte(sink,ctx,9);return;}
    ch=printable(key,shift,s->caps_lock);
    if(!ch) return;
    if(ctrl) {
        /* All letter keys map to ASCII control codes, irrespective of CapsLock. */
        if(key>=0x04 && key<=0x1d)ch=(unsigned char)(1+key-0x04);
        else if(ch==' ' || ch=='@')ch=0;
        else if(ch=='[' || ch=='{')ch=27;
        else if(ch=='\\' || ch=='|')ch=28;
        else if(ch==']' || ch=='}')ch=29;
        else if(ch=='^')ch=30;
        else if(ch=='_')ch=31;
        else if(ch=='?')ch=127;
        else return;
    }
    if(alt)emit_byte(sink,ctx,27); /* Alt as Meta prefix */
    emit_byte(sink,ctx,ch);
}

void xssh_hid_keys_reset(XsshHidKeys *s) {memset(s,0,sizeof(*s));}

void xssh_hid_keys_report(XsshHidKeys *s,const uint8_t report[8],
                          uint32_t now_ms,XsshKeySink sink,void *ctx) {
    uint8_t next_repeat=0;
    /* HID rollover/error usages 0x01..0x03 must not be treated as releases. */
    for(int i=2;i<8;i++)if(report[i]>=1 && report[i]<=3)return;
    for(int i=2;i<8;i++) {
        uint8_t key=report[i];
        if(!key || in_report(s->previous,key))continue;
        send_key(s,key,report[0],sink,ctx);
        if(key!=0x39 && key!=0x53)next_repeat=key;
    }
    if(next_repeat) {
        s->repeat_key=next_repeat;
        s->repeat_at=now_ms+REPEAT_DELAY;
    } else if(s->repeat_key && !in_report(report,s->repeat_key)) {
        s->repeat_key=0;
    }
    memcpy(s->previous,report,8);
}

void xssh_hid_keys_tick(XsshHidKeys *s,uint32_t now_ms,
                        XsshKeySink sink,void *ctx) {
    if(!s->repeat_key || !in_report(s->previous,s->repeat_key))return;
    if((int32_t)(now_ms-s->repeat_at)>=0) {
        send_key(s,s->repeat_key,s->previous[0],sink,ctx);
        s->repeat_at=now_ms+REPEAT_INTERVAL;
    }
}
