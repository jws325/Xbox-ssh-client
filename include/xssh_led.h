#ifndef XSSH_LED_H
#define XSSH_LED_H
#include <stdint.h>
typedef enum { XSSH_LED_AUTO, XSSH_LED_GREEN, XSSH_LED_RED,
 XSSH_LED_ORANGE, XSSH_LED_OFF, XSSH_LED_BLINK_GREEN,
 XSSH_LED_BLINK_RED, XSSH_LED_ALTERNATE } XsshLedMode;
int xssh_led_parse(const char *s, XsshLedMode *out);
/* Four pairs: green/red bits for four successive 250ms SMC phases. */
uint8_t xssh_led_pattern(XsshLedMode mode, int state);
void xssh_led_apply(uint8_t pattern);
void xssh_led_restore(void);
#endif
