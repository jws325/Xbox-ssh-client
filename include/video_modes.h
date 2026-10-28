#ifndef XSSH_VIDEO_MODES_H
#define XSSH_VIDEO_MODES_H

#include <stddef.h>

/* Portable resolution parser and selector. The Xbox HAL backend supplies only
   modes actually exposed by XVideoListModes; arbitrary requests are never
   programmed directly into scanout registers. */
#define XSSH_MAX_VIDEO_MODES 512

typedef struct {
    int width;
    int height;
    int refresh;
    int progressive; /* 0=interlaced/SD, 1=progressive, -1=unspecified */
} XsshVideoMode;

typedef struct {
    XsshVideoMode modes[XSSH_MAX_VIDEO_MODES];
    size_t count;
} XsshVideoModeList;

typedef struct {
    int width;
    int height;
    int refresh;    /* 0 = any supported refresh */
    int progressive;/* -1 = any; 0 = i; 1 = p */
    int automatic;
    int allow_480_width; /* preset 480i/p accepts 640 or 720 active pixels */
} XsshVideoRequest;

/* Accept auto, 480i, 480p, 720p, 1080i, 576i, or WxH[@Hz],
   including custom resolutions. A trailing p/i is accepted on WxH;
   presets also accept @Hz (e.g. 720p@60). */
int xssh_video_parse(const char *name, XsshVideoRequest *out);
int xssh_video_append(XsshVideoModeList *list, XsshVideoMode mode);
/* Choose a mode matching the active display dimensions at XBE startup.
   Exact width/height are required; prefer inherited refresh when known.
   Returns -1 if the live resolution is not exposed by the HAL. */
int xssh_video_select_startup(const XsshVideoModeList *list,
                              int width, int height, int refresh);
/* Explicit video_mode requests only. Auto is resolved by the Xbox backend
   using the inherited display dimensions, not by this selector. */
int xssh_video_select(const XsshVideoModeList *list,
                      const XsshVideoRequest *request);
int xssh_video_fallback(const XsshVideoModeList *list);
void xssh_video_describe(XsshVideoMode mode, char *out, size_t outsz);

#endif
