/* Run: cc -std=c11 -Wall -Wextra -Werror -Iinclude src/hid_keys.c tools/test-hid-keys.c -o /tmp/xssh-hid-test && /tmp/xssh-hid-test */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hid_keys.h"

typedef struct {XsshKeyEvent events[64];int count;} Capture;
static void collect(void *ctx,XsshKeyEvent ev) {
    Capture *c=ctx;
    assert(c->count < (int)(sizeof(c->events)/sizeof(c->events[0])));
    c->events[c->count++]=ev;
}
static void report(XsshHidKeys *s,Capture *c,uint8_t mod,uint8_t k,uint32_t ms) {
    uint8_t r[8]={0};r[0]=mod;r[2]=k;
    xssh_hid_keys_report(s,r,ms,collect,c);
}
static void check_byte(Capture *c,int i,int v) {
    assert(c->events[i].kind==XSSH_KEY_TEXT);
    assert(c->events[i].byte==v);
}
static void check_seq(Capture *c,int i,const char *s) {
    assert(c->events[i].kind==XSSH_KEY_SEQUENCE);
    assert(!strcmp(c->events[i].sequence,s));
}
int main(void) {
    XsshHidKeys s;Capture c={0};
    xssh_hid_keys_reset(&s);
    report(&s,&c,0,0x04,0);           /* a */
    assert(c.count==1);check_byte(&c,0,'a');
    report(&s,&c,0,0x04,10);          /* no duplicate on same report */
    assert(c.count==1);
    xssh_hid_keys_tick(&s,400,collect,&c);assert(c.count==1);
    xssh_hid_keys_tick(&s,440,collect,&c);check_byte(&c,1,'a');
    xssh_hid_keys_tick(&s,485,collect,&c);check_byte(&c,2,'a');
    report(&s,&c,0,0,490);
    xssh_hid_keys_tick(&s,2000,collect,&c);assert(c.count==3);
    report(&s,&c,0x22,0x04,2100);check_byte(&c,3,'A');
    report(&s,&c,0,0,2110);
    report(&s,&c,0x11,0x06,2120);check_byte(&c,4,3); /* Ctrl+C */
    report(&s,&c,0,0,2130);
    report(&s,&c,0x44,0x1b,2140);check_byte(&c,5,27);check_byte(&c,6,'x'); /* Alt+x */
    report(&s,&c,0,0,2150);
    report(&s,&c,0,0x4f,2160);check_seq(&c,7,"\033[C");
    report(&s,&c,0,0,2170);
    report(&s,&c,0x22,0x2b,2180);check_seq(&c,8,"\033[Z");
    report(&s,&c,0,0,2190);
    report(&s,&c,0,0x2d,2200);check_byte(&c,9,'-');
    report(&s,&c,0,0,2210);
    report(&s,&c,0x22,0x2d,2220);check_byte(&c,10,'_');
    report(&s,&c,0,0,2230);
    report(&s,&c,0,0x33,2240);check_byte(&c,11,';');
    report(&s,&c,0,0,2250);
    report(&s,&c,0,0x38,2260);check_byte(&c,12,'/');
    report(&s,&c,0,0,2270);
    report(&s,&c,0,0x39,2280);assert(c.count==13); /* Caps lock toggles */
    report(&s,&c,0,0,2290);
    report(&s,&c,0,0x04,2300);check_byte(&c,13,'A');
    report(&s,&c,0,0,2310);
    report(&s,&c,0x22,0x04,2320);check_byte(&c,14,'a');
    report(&s,&c,0,0,2330);
    report(&s,&c,0,0x28,2340);assert(c.events[15].kind==XSSH_KEY_ENTER);
    report(&s,&c,0,0,2350);
    report(&s,&c,0,0x2a,2360);assert(c.events[16].kind==XSSH_KEY_BACKSPACE);
    report(&s,&c,0,0,2370);
    report(&s,&c,0,0x3a,2380);check_seq(&c,17,"\033OP");
    report(&s,&c,0,0,2390);
    report(&s,&c,0,0x4c,2400);check_seq(&c,18,"\033[3~");
    report(&s,&c,0,0,2410);
    report(&s,&c,0,0x59,2420);check_byte(&c,19,'1'); /* keypad */
    report(&s,&c,0,0,2430);
    report(&s,&c,0,0x53,2440);assert(c.count==20); /* Num Lock ignored */
    report(&s,&c,0,0,2450);
    {
        uint8_t invalid[8]={0};invalid[2]=0x01;
        xssh_hid_keys_report(&s,invalid,2460,collect,&c);
        assert(c.count==20); /* rollover preserved */
    }
    printf("HID keyboard decode tests: OK (%d emitted events)\n",c.count);
    return 0;
}
