#include "video_modes.h"
#include "video_xbox.h"
#include <hal/video.h>
#include <hal/xbox.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static VIDEO_MODE supported[12];
static size_t supported_n;
static DWORD encoder;
static VIDEO_MODE selected;
static VIDEO_MODE inherited_mode;
static int fail_width;
static int startup_width, startup_height;
/* Simulated NV2A PRAMDAC active-display readout. */
unsigned long xssh_test_video_reg(unsigned offset) {
    if (offset == 0x00680838u) return (unsigned long)(startup_width - 1);
    if (offset == 0x00680818u) return (unsigned long)(startup_height - 1);
    return 0xfffffffful;
}
static void startup(int w, int h) { startup_width=w; startup_height=h; }


DWORD XVideoGetEncoderSettings(void) { return encoder; }
int XVideoListModes(VIDEO_MODE *out, int bpp, int refresh, void **position) {
    size_t i = (size_t)(uintptr_t)(*position);
    for (; i < supported_n; ++i) {
        if (supported[i].refresh != refresh) continue;
        *out = supported[i]; out->bpp=bpp;
        *position = (void *)(uintptr_t)(i+1);
        return 1;
    }
    *position=(void *)(uintptr_t)supported_n;
    return 0;
}
int XVideoSetMode(int w, int h, int bpp, int hz) {
    for(size_t i=0;i<supported_n;i++) {
        if(supported[i].width==w && supported[i].height==h &&
           supported[i].refresh==(hz ? hz : supported[i].refresh) && w!=fail_width) {
            selected=supported[i]; selected.bpp=bpp;
            return 1;
        }
    }
    return 0;
}
VIDEO_MODE XVideoGetMode(void) { return inherited_mode; }
static void add(int w, int h, int hz) {
    supported[supported_n++]=(VIDEO_MODE){w,h,32,hz};
}
static void reset(void) {
    supported_n=0; encoder=AV_PACK_HDTV|VIDEO_MODE_480P;
    memset(&selected,0,sizeof(selected));memset(&inherited_mode,0,sizeof(inherited_mode));fail_width=0;startup(0,0);
}
static void test(const char *mode, int w, int h, int hz, int warning) {
    XsshVideoActive a;
    assert(xssh_video_initialize(mode,&a));
    assert(a.width==w && a.height==h && a.refresh==hz);
    assert(!!a.warning[0] == !!warning);
    assert(selected.width==w && selected.height==h);
}
int main(void) {
    XsshVideoRequest r;
    assert(xssh_video_parse("1920x1080@30",&r) && r.width==1920 && r.height==1080 && r.refresh==30);
    assert(xssh_video_parse("640x480i@50",&r) && r.progressive==0 && r.refresh==50);
    assert(xssh_video_parse("800x600p@75",&r) && r.progressive==1 && r.refresh==75);
    assert(xssh_video_parse("auto",&r) && r.automatic);
    assert(xssh_video_parse("720p@60",&r) && r.width==1280 && r.refresh==60);
    assert(xssh_video_parse("1080i@60",&r) && r.progressive==0 && r.refresh==60);
    assert(!xssh_video_parse("640x480@999999999",&r));
    assert(!xssh_video_parse("480pp",&r));
    assert(!xssh_video_parse("65535x480",&r));
    assert(!xssh_video_parse("0x480",&r));
    assert(!xssh_video_parse("auto@60",&r));
    assert(!xssh_video_parse("720p@999",&r));
    reset(); add(640,480,60); add(720,480,60); add(1280,720,60); add(1920,1080,60);
    startup(1280,720);
    test("auto",1280,720,60,0);  /* inherits 720p, not lowest 480p */
    startup(1920,1080);
    test("auto",1920,1080,60,0); /* inherits 1080i, not lowest 480p */
    startup(640,480);
    test("auto",640,480,60,0);
    test("480p",640,480,60,0);
    test("720p",1280,720,60,0);
    test("720p@60",1280,720,60,0);
    test("1080i",1920,1080,60,0);
    test("1280x720@60",1280,720,60,0);
    test("640x480@60",640,480,60,0);
    test("480i",640,480,60,1); /* not actually listed on HDTV when 480p on! */
    test("1440x900@60",640,480,60,1);
    test("garbage",640,480,60,1);
    fail_width=1280; test("720p",640,480,60,1);
    reset(); encoder=AV_PACK_HDTV; add(640,480,60); add(1280,720,60);
    test("480i",640,480,60,0);
    test("480p",640,480,60,1);
    reset(); encoder=AV_PACK_VGA; add(800,600,75); add(1024,768,60);
    test("800x600@75",800,600,75,0);
    test("1024x768p@60",1024,768,60,0);
    startup(1024,768);
    test("auto",1024,768,60,0);
    startup(800,600);
    test("auto",800,600,75,0);
    reset(); encoder=AV_PACK_VGA; add(640,480,60); add(960,540,60); add(1920,1080,30);
    test("960x540@60",960,540,60,0);
    test("1920x1080@30",1920,1080,30,0);
    /* If HAL already tracks the inherited mode, use it over MMIO guess. */
    reset(); add(640,480,60); add(1280,720,60);
    inherited_mode=(VIDEO_MODE){1280,720,32,60}; startup(640,480);
    test("auto",1280,720,60,0);
    /* Stale/invalid hardware registers: safe fallback, with warning. */
    reset(); add(640,480,60); add(1280,720,60);
    test("auto",640,480,60,1);
    /* Startup custom mode cannot be forced if HAL does not expose it. */
    startup(1600,900);
    test("auto",640,480,60,1);
    /* No listed 640x480 mode: fall back to compatible 720p when needed. */
    reset(); add(1280,720,60); startup(1600,900);
    test("auto",1280,720,60,1);
    /* A custom startup mode should not be replaced with 640x480. */
    reset(); add(640,480,60); add(1024,768,75); startup(1024,768);
    test("auto",1024,768,75,0);
    printf("XSSH video mode parser, dashboard-inherit and HAL fallback tests passed\n");
    return 0;
}
