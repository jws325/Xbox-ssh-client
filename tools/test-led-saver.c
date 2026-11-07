#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xssh_led.h"
#include "xssh_saver.h"
int main(int argc,char**argv){
 XsshLedMode mode;
 assert(xssh_led_parse("alternate",&mode) && xssh_led_pattern(mode,1)==0xa5);
 assert(xssh_led_parse("green",&mode) && xssh_led_pattern(mode,1)==0x0f);
 assert(xssh_led_parse("auto",&mode) && xssh_led_pattern(mode,1)==0x0f);
 assert(xssh_led_pattern(mode,4)==0);
 assert(!xssh_led_parse("purple",&mode));
 assert(argc==2);
 XsshSaver s;
 assert(xssh_saver_open(&s,argv[1]));
 assert(s.width==160&&s.height==120&&s.fps==12&&s.frames==48);
 assert(xssh_saver_frame(&s,1));
 unsigned char before[2]={s.pixels[0],s.pixels[1]};
 assert(xssh_saver_frame(&s,1+48));
 assert(s.pixels[0]==before[0]&&s.pixels[1]==before[1]);
 xssh_saver_close(&s);
 puts("PASS: LED mode, RGB565 header, frame playback, looping");
 return 0;
}
