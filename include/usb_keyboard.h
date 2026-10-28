#ifndef XSSH_USB_KEYBOARD_H
#define XSSH_USB_KEYBOARD_H
#include <stdint.h>
#include "hid_keys.h"

/* SDL game-controller initialization owns usbh_core_init().  Call this once
   immediately afterwards, before allowing any USB hub polling/enumeration.
   See Makefile's SDL_DISABLE_JOYSTICK_INIT_DELAY setting. */
void xssh_usb_keyboard_init(void);
/* Called once per UI frame, after SDL_PollEvent() has polled USB hubs. */
void xssh_usb_keyboard_poll(uint32_t now_ms,XsshKeySink sink,void *ctx);
int xssh_usb_keyboard_connected(void);
/* On-screen software keyboard is only shown when no USB boot keyboard is active. */
int xssh_software_keyboard_visible(void);
#endif
