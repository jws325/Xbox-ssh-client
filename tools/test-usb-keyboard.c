/* Native integration test with mocked nxdk USB HID host API. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "usb_keyboard.h"
#include "usbh_lib.h"

static HID_DEV_T device;
static HID_IR_FUNC *callback;
static int init_calls,read_calls,protocol_calls;
static int present;
static char output[64];
static int output_len;

void usbh_hid_init(void) {init_calls++;}
HID_DEV_T *usbh_hid_get_device_list(void) {return present?&device:NULL;}
int usbh_hid_set_protocol(HID_DEV_T *d,uint8_t protocol) {
    assert(d==&device); assert(protocol==0);protocol_calls++;return 0;
}
int usbh_hid_start_int_read(HID_DEV_T *d,uint8_t ep,HID_IR_FUNC *fn) {
    assert(d==&device);assert(ep==0);read_calls++;callback=fn;return 0;
}
static void sink(void *ctx,XsshKeyEvent ev) {
    (void)ctx;
    if(ev.kind==XSSH_KEY_TEXT)output[output_len++]=(char)ev.byte;
    else if(ev.kind==XSSH_KEY_ENTER)output[output_len++]='\n';
}
static void inject(uint8_t key) {
    uint8_t r[8]={0};r[2]=key;
    assert(callback);callback(&device,0,0,r,8);
}
int main(void) {
    xssh_usb_keyboard_init();
    assert(init_calls==1 && !xssh_usb_keyboard_connected());
    assert(xssh_software_keyboard_visible());
    device.iface=&device;device.bSubClassCode=1;device.bProtocolCode=2;
    present=1;
    xssh_usb_keyboard_poll(0,sink,NULL);
    assert(!xssh_usb_keyboard_connected() && read_calls==0); /* mouse not keyboard */
    assert(xssh_software_keyboard_visible());
    device.bProtocolCode=1;
    xssh_usb_keyboard_poll(1,sink,NULL);
    assert(xssh_usb_keyboard_connected() && read_calls==1 && protocol_calls==1);
    assert(!xssh_software_keyboard_visible()); /* auto-hide on connect */
    inject(0x04);inject(0);inject(0x05);
    xssh_usb_keyboard_poll(3,sink,NULL);
    assert(output_len==2 && !memcmp(output,"ab",2));
    present=0;device.iface=NULL;
    xssh_usb_keyboard_poll(4,sink,NULL);
    assert(!xssh_usb_keyboard_connected());
    assert(xssh_software_keyboard_visible()); /* restore on unplug */
    device.iface=&device;present=1;
    xssh_usb_keyboard_poll(5,sink,NULL);
    assert(xssh_usb_keyboard_connected() && read_calls==2);
    assert(!xssh_software_keyboard_visible()); /* hide again on replug */
    inject(0x04);
    xssh_usb_keyboard_poll(6,sink,NULL);
    assert(output_len==3 && output[2]=='a'); /* fresh key state */
    puts("USB host glue tests: OK (attach/boot/protocol/packets/unplug/replug/virtual keyboard visibility)");
    return 0;
}
