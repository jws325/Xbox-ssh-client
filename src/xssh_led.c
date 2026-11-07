#include "xssh_led.h"
#include <string.h>
#ifdef NXDK
#include <hal/led.h>
#endif
int xssh_led_parse(const char *s, XsshLedMode *out) {
 static const char *names[]={"auto","green","red","orange","off","blink_green","blink_red","alternate"};
 if(!s || !out)return 0;
 for(int i=0;i<8;i++)if(strcmp(s,names[i])==0){*out=(XsshLedMode)i;return 1;}
 return 0;
}
uint8_t xssh_led_pattern(XsshLedMode m,int state) {
 if(m==XSSH_LED_AUTO) {
   /* 0 startup, 1 connected, 2 verifying, 3 error, 4 screensaver */
   m=state==1?XSSH_LED_GREEN:state==3?XSSH_LED_RED:
     state==4?XSSH_LED_OFF:XSSH_LED_ORANGE;
 }
 switch(m){
 case XSSH_LED_GREEN:return 0x0f;
 case XSSH_LED_RED:return 0xf0;
 case XSSH_LED_ORANGE:return 0xff;
 case XSSH_LED_OFF:return 0;
 case XSSH_LED_BLINK_GREEN:return 0x05;
 case XSSH_LED_BLINK_RED:return 0x50;
 case XSSH_LED_ALTERNATE:return 0xa5;
 default:return 0x0f;
 }
}
void xssh_led_apply(uint8_t pattern) {
#ifdef NXDK
 /* XLED_GREEN=1, XLED_RED=16; SMC cycles through four LED phases. */
 XLEDColor phase[4];
 for(int i=0;i<4;i++) {
    int g=(pattern>>(3-i))&1;
    int r=(pattern>>(7-i))&1;
    phase[i]=(XLEDColor)((g?XLED_GREEN:0)|(r?XLED_RED:0));
 }
 XSetCustomLED(phase[0],phase[1],phase[2],phase[3]);
#endif
 (void)pattern;
}
void xssh_led_restore(void){
#ifdef NXDK
 XResetLED();
#endif
}
