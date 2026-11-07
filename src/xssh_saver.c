/* XSV1: 8-byte magic XSSHRGB5, followed by four LE32s (w,h,fps,count),
   followed by count * (w*h*2) contiguous, raw RGB565-LE frames.
   Designed for constant-time playback from the Xbox hard drive, zero codecs. */
#include "xssh_saver.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static uint32_t le32(const unsigned char *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
void xssh_saver_close(XsshSaver *s){if(!s)return;if(s->file)fclose(s->file);free(s->pixels);memset(s,0,sizeof(*s));}
int xssh_saver_open(XsshSaver *s,const char *path){
 if(!s||!path)return 0;
 memset(s,0,sizeof(*s));
 s->file=fopen(path,"rb");if(!s->file)return 0;
 unsigned char h[XSSH_SAVER_HEADER];
 if(fread(h,1,sizeof h,s->file)!=sizeof h || memcmp(h,"XSSHRGB5",8)){xssh_saver_close(s);return 0;}
 s->width=le32(h+8);s->height=le32(h+12);s->fps=le32(h+16);s->frames=le32(h+20);
 if(!s->width||!s->height||s->width>XSSH_SAVER_MAX_WIDTH||s->height>XSSH_SAVER_MAX_HEIGHT||
    s->fps<1||s->fps>30||!s->frames||s->frames>1000000u){xssh_saver_close(s);return 0;}
 size_t bytes=(size_t)s->width*s->height*2;
 if(bytes>UINT32_MAX / s->frames){xssh_saver_close(s);return 0;}
 /* Check the file length so partial videos do not lock the render loop. */
 if(fseek(s->file,0,SEEK_END)!=0){xssh_saver_close(s);return 0;}
 long actual=ftell(s->file);
 if(actual<0||(unsigned long)actual < XSSH_SAVER_HEADER+(unsigned long)bytes*s->frames){xssh_saver_close(s);return 0;}
 s->pixels=(uint8_t*)malloc(bytes);
 if(!s->pixels){xssh_saver_close(s);return 0;}
 return 1;
}
int xssh_saver_frame(XsshSaver *s,uint32_t frame){
 if(!s||!s->file||!s->pixels||!s->frames)return 0;
 frame%=s->frames;
 unsigned long offset=XSSH_SAVER_HEADER+(unsigned long)frame*s->width*s->height*2;
 if(offset>LONG_MAX||fseek(s->file,(long)offset,SEEK_SET)!=0)return 0;
 if(fread(s->pixels,1,(size_t)s->width*s->height*2,s->file)!=(size_t)s->width*s->height*2)return 0;
 s->current_frame=frame;
 return 1;
}
