#ifndef XSSH_HID_KEYS_H
#define XSSH_HID_KEYS_H

#include <stdint.h>

/* A USB HID boot-keyboard report is 1 modifier byte, 1 reserved byte,
   and up to six currently-held USB usage IDs.  This decoder is portable. */
typedef enum {
    XSSH_KEY_TEXT, XSSH_KEY_ENTER, XSSH_KEY_BACKSPACE,
    XSSH_KEY_SEQUENCE
} XsshKeyKind;

typedef struct {
    XsshKeyKind kind;
    unsigned char byte;        /* for TEXT; may be an ASCII control byte */
    const char *sequence;      /* for SEQUENCE; static string */
} XsshKeyEvent;

typedef void (*XsshKeySink)(void *ctx, XsshKeyEvent event);

typedef struct {
    uint8_t previous[8];
    uint8_t repeat_key;
    uint8_t caps_lock;
    uint32_t repeat_at;
} XsshHidKeys;

void xssh_hid_keys_reset(XsshHidKeys *state);
void xssh_hid_keys_report(XsshHidKeys *state, const uint8_t report[8],
                          uint32_t now_ms, XsshKeySink sink, void *ctx);
void xssh_hid_keys_tick(XsshHidKeys *state, uint32_t now_ms,
                        XsshKeySink sink, void *ctx);
#endif
