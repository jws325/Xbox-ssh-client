#include "video_modes.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int parse_natural(const char **src, int *out) {
    const char *p = *src;
    unsigned int n = 0;
    if (!isdigit((unsigned char)*p)) return 0;
    do {
        n = n * 10 + (unsigned)(*p - '0');
        if (n > 10000) return 0;
        ++p;
    } while (isdigit((unsigned char)*p));
    if (!n) return 0;
    *out = (int)n;
    *src = p;
    return 1;
}

int xssh_video_parse(const char *name, XsshVideoRequest *out) {
    XsshVideoRequest r = {0, 0, 0, -1, 0, 0};
    const char *p;
    int w, h;
    if (!name || !out) return 0;
    /* Optional @Hz works for presets as well as numeric modes. Split exactly
       once so 720p@60 and 1280x720@60 share the same validation. */
    const char *at = strchr(name, '@');
    if (at) {
        char base[48];
        int hz;
        size_t n = (size_t)(at - name);
        if (n == 0 || n >= sizeof(base)) return 0;
        memcpy(base, name, n); base[n] = 0;
        p = at + 1;
        if (!parse_natural(&p, &hz) || *p || hz > 240) return 0;
        if (!xssh_video_parse(base, &r) || r.automatic) return 0;
        r.refresh = hz;
        *out = r;
        return 1;
    }
    if (!strcmp(name, "auto")) { r.automatic = 1; *out = r; return 1; }
    if (!strcmp(name, "480i")) { r.width = 640; r.height = 480; r.progressive = 0; r.allow_480_width = 1; *out = r; return 1; }
    if (!strcmp(name, "480p")) { r.width = 640; r.height = 480; r.progressive = 1; r.allow_480_width = 1; *out = r; return 1; }
    if (!strcmp(name, "576i")) { r.width = 720; r.height = 576; r.progressive = 0; r.refresh = 50; *out = r; return 1; }
    if (!strcmp(name, "720p")) { r.width = 1280; r.height = 720; r.progressive = 1; *out = r; return 1; }
    if (!strcmp(name, "1080i")) { r.width = 1920; r.height = 1080; r.progressive = 0; *out = r; return 1; }
    p = name;
    if (!parse_natural(&p, &w) || (*p != 'x' && *p != 'X')) return 0;
    ++p;
    if (!parse_natural(&p, &h)) return 0;
    if (*p == 'p') { r.progressive = 1; ++p; }
    else if (*p == 'i') { r.progressive = 0; ++p; }
    if (*p) return 0;
    if (w < 320 || h < 200 || w > 4096 || h > 2160) return 0;
    r.width = w; r.height = h;
    *out = r;
    return 1;
}

int xssh_video_append(XsshVideoModeList *list, XsshVideoMode mode) {
    if (!list || mode.width <= 0 || mode.height <= 0) return 0;
    for (size_t i = 0; i < list->count; ++i) {
        const XsshVideoMode *m = &list->modes[i];
        if (m->width == mode.width && m->height == mode.height &&
            m->refresh == mode.refresh && m->progressive == mode.progressive) return 1;
    }
    if (list->count >= XSSH_MAX_VIDEO_MODES) return 0;
    list->modes[list->count++] = mode;
    return 1;
}

static int best_at(const XsshVideoModeList *list, int w, int h, int scan) {
    int best = -1;
    for (size_t i = 0; i < list->count; ++i) {
        const XsshVideoMode *m = &list->modes[i];
        if (m->width != w || m->height != h || (scan != -1 && m->progressive != scan)) continue;
        if (best < 0 || (m->refresh == 60 && list->modes[best].refresh != 60)) best = (int)i;
    }
    return best;
}

int xssh_video_select_startup(const XsshVideoModeList *list,
                              int width, int height, int refresh) {
    int best = -1;
    if (!list || width < 320 || height < 200 || width > 4096 || height > 2160)
        return -1;
    for (size_t i = 0; i < list->count; ++i) {
        const XsshVideoMode *m = &list->modes[i];
        if (m->width != width || m->height != height) continue;
        if (best < 0 || (refresh > 0 && m->refresh == refresh &&
                         list->modes[best].refresh != refresh)) best = (int)i;
    }
    return best;
}

int xssh_video_fallback(const XsshVideoModeList *list) {
    int n;
    if (!list || !list->count) return -1;
    n = best_at(list, 640, 480, -1);
    if (n >= 0) return n;
    n = best_at(list, 720, 480, -1);
    if (n >= 0) return n;
    /* Prefer the smallest mode to limit video RAM if 640x480 isn't offered. */
    n = 0;
    for (size_t i = 1; i < list->count; ++i) {
        const XsshVideoMode *a = &list->modes[i], *b = &list->modes[n];
        if ((long)a->width * a->height < (long)b->width * b->height) n = (int)i;
    }
    return n;
}

int xssh_video_select(const XsshVideoModeList *list, const XsshVideoRequest *request) {
    if (!list || !request || !list->count) return -1;
    if (request->automatic) return -1; /* Hardware startup probe required. */
    int best = -1;
    for (size_t i = 0; i < list->count; ++i) {
        const XsshVideoMode *m = &list->modes[i];
        int width_match = m->width == request->width;
        /* The standard 480i/480p aliases accept both 640x480 and 720x480. */
        if (request->allow_480_width && m->width == 720) width_match = 1;
        if (!width_match || m->height != request->height) continue;
        if (request->refresh && m->refresh != request->refresh) continue;
        if (request->progressive != -1 && m->progressive != request->progressive) continue;
        if (best < 0 ||
            (m->width == request->width && list->modes[best].width != request->width) ||
            (m->width == list->modes[best].width && m->refresh == 60 && list->modes[best].refresh != 60))
            best = (int)i;
    }
    return best;
}

void xssh_video_describe(XsshVideoMode mode, char *out, size_t outsz) {
    if (!out || !outsz) return;
    snprintf(out, outsz, "%dx%d%c@%d", mode.width, mode.height,
             mode.progressive == 1 ? 'p' : mode.progressive == 0 ? 'i' : '?', mode.refresh);
}
