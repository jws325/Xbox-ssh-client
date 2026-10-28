#ifndef XSSH_VIDEO_XBOX_H
#define XSSH_VIDEO_XBOX_H
#include <stddef.h>

typedef struct {
    int width;
    int height;
    int refresh;
    char description[48];
    char warning[160];
} XsshVideoActive;
/* Read the startup NV2A scanout before configuring the display; for auto,
   match that starting width/height against nxdk's available HAL modes.
   Fallback to a safe output only when inherited/requested mode is unusable.
   Returns 1 only if an XVideoSetMode call succeeds. */
int xssh_video_initialize(const char *requested, XsshVideoActive *active);
#endif
