#include "video_xbox.h"
#include "video_modes.h"
#include <hal/video.h>
#include <hal/xbox.h>
#include <stdio.h>
#include <string.h>

/* Read the already-active scanout size before XVideoSetMode() or SDL
   reprograms the video hardware. nxdk's XVideoGetMode() is zero before its
   first local XVideoSetMode() call, so we also inspect the live NV2A RAMDAC
   H/V valid-end registers (documented by the ReactOS Xbox video driver).
   These give ACTIVE PIXELS, not the dashboard's saved list of allowed modes.
   They may already contain a BIOS/launcher-reset mode after XBE launch. */
#define NV2A_FP_HVALID_END 0x00680838u
#define NV2A_FP_VVALID_END 0x00680818u
#define PAL_VIDEO_REGION  0x00000300u
#define PAL_VIDEO_60HZ    0x00400000u

static int startup_display(int *width, int *height, int *refresh) {
    VIDEO_MODE current = XVideoGetMode();
    DWORD encoder = XVideoGetEncoderSettings();
    unsigned w, h;
    if (current.width >= 320 && current.width <= 4096 &&
        current.height >= 200 && current.height <= 2160) {
        *width = current.width;
        *height = current.height;
        *refresh = current.refresh;
        return 1;
    }
    /* Use unsigned so 0xffffffff wraps to zero and is safely rejected. */
    w = (unsigned)VIDEOREG(NV2A_FP_HVALID_END) + 1u;
    h = (unsigned)VIDEOREG(NV2A_FP_VVALID_END) + 1u;
    if (w < 320 || w > 4096 || h < 200 || h > 2160) return 0;
    *width = (int)w;
    *height = (int)h;
    /* nxdk exposes regional 50/60Hz flags, but not the precise refresh of
       arbitrary custom BIOS timings. A same-sized available mode wins even
       when its refresh differs from this hint. */
    *refresh = ((encoder & VIDEO_STANDARD_MASK) == PAL_VIDEO_REGION &&
                !(encoder & PAL_VIDEO_60HZ)) ? 50 : 60;
    return 1;
}

/* The HAL returns width/height/refresh but no scan-type field.  For 480-line
   component modes, nxdk's enumerator exposes 480p *instead of* 480i when the
   Dashboard has 480p enabled (video.c). For VGA, scanout is progressive. */
static int scan_type(const VIDEO_MODE *vm, DWORD encoder) {
    int av = (int)(encoder & VIDEO_ADAPTER_MASK);
    if (vm->height == 720) return 1;
    if (vm->height == 1080) return vm->refresh == 60 ? 0 : -1;
    if (vm->height == 576 && av != AV_PACK_VGA) return 0;
    if (vm->height == 480) {
        if (av == AV_PACK_VGA) return 1;
        if (av == AV_PACK_HDTV && (encoder & VIDEO_MODE_480P)) return 1;
        return 0;
    }
    if (av == AV_PACK_VGA) return 1;
    /* For custom modes the stock HAL doesn't expose a scan type. Never
       falsely certify an arbitrary scan format as progressive/interlaced. */
    return -1;
}

static void enumerate_modes(XsshVideoModeList *list) {
    DWORD encoder = XVideoGetEncoderSettings();
    /* XVideoListModes refresh=0 chooses the current video refresh, *not* all.
       Search integer rates so compatible nxdk forks can expose extra timing
       values, without hard-coding a width/height whitelist. */
    for (int refresh = 24; refresh <= 240; ++refresh) {
        void *iter = NULL;
        VIDEO_MODE m;
        while (XVideoListModes(&m, 32, refresh, &iter)) {
            XsshVideoMode v = {m.width, m.height, m.refresh, scan_type(&m, encoder)};
            if (!xssh_video_append(list, v)) return;
        }
    }
}

int xssh_video_initialize(const char *requested, XsshVideoActive *active) {
    XsshVideoModeList list = {0};
    XsshVideoRequest parsed;
    int match = -1, fallback, selected = -1, startup_w = 0, startup_h = 0, startup_hz = 0;
    int have_startup;
    int valid = xssh_video_parse(requested ? requested : "auto", &parsed);
    if (!active) return 0;
    memset(active, 0, sizeof(*active));
    if (!valid) xssh_video_parse("auto", &parsed);

    /* IMPORTANT: probe before invoking XVideoSetMode, which overwrites the
       startup state. Do not replace this with XVideoGetMode() alone. */
    have_startup = startup_display(&startup_w, &startup_h, &startup_hz);
    enumerate_modes(&list);

    if (parsed.automatic) {
        if (have_startup) {
            match = xssh_video_select_startup(&list, startup_w, startup_h, startup_hz);
            if (match < 0) {
                snprintf(active->warning, sizeof(active->warning),
                         "Startup display %dx%d not listed by HAL; using safe fallback",
                         startup_w, startup_h);
            }
        } else {
            snprintf(active->warning, sizeof(active->warning),
                     "Startup display could not be read; using safe fallback");
        }
    } else {
        match = xssh_video_select(&list, &parsed);
        if (match < 0)
            snprintf(active->warning, sizeof(active->warning),
                     "Video mode '%.*s' unavailable; using safe fallback", 48,
                     requested ? requested : "");
    }
    if (!valid)
        snprintf(active->warning, sizeof(active->warning),
                 "Invalid video_mode '%.*s'; trying startup display", 48,
                 requested ? requested : "");

    if (match >= 0) {
        XsshVideoMode m = list.modes[match];
        if (XVideoSetMode(m.width, m.height, 32, m.refresh)) selected = match;
        else snprintf(active->warning, sizeof(active->warning),
                      "Failed to set requested/startup display; using safe fallback");
    }

    /* A fallback is only a last resort; video_mode=auto NEVER chooses the
       smallest mode while its inherited resolution is usable. */
    fallback = xssh_video_fallback(&list);
    if (selected < 0 && fallback >= 0) {
        XsshVideoMode m = list.modes[fallback];
        if (XVideoSetMode(m.width, m.height, 32, m.refresh)) selected = fallback;
    }
    if (selected < 0) {
        /* No supported enumerated mode succeeded. */
        if (!XVideoSetMode(640, 480, 32, REFRESH_DEFAULT)) return 0;
        active->width = 640; active->height = 480;
        VIDEO_MODE m = XVideoGetMode();
        active->refresh = m.refresh;
        snprintf(active->description, sizeof(active->description),
                 "640x480@%d", active->refresh);
        snprintf(active->warning, sizeof(active->warning),
                 "No usable enumerated modes; recovered at 640x480");
        return 1;
    }
    XsshVideoMode mode = list.modes[selected];
    active->width = mode.width;
    active->height = mode.height;
    active->refresh = mode.refresh;
    xssh_video_describe(mode, active->description, sizeof(active->description));
    return 1;
}
