/* Direct USB HID Boot Keyboard input for original Xbox / nxdk.
   nxdk SDL2 has a gamepad driver but no Xbox HID keyboard SDL backend.
   Uses the same initialized USB host core as the SDL gamepad driver. */
#include "usb_keyboard.h"
#include <string.h>
#include <usbh_lib.h>
#include <usbh_hid.h>

#define REPORT_QUEUE 64u /* must be a power of two */

typedef struct {
    HID_DEV_T *dev;
    uint8_t report[8];
} QueuedReport;

static QueuedReport queue[REPORT_QUEUE];
static unsigned int queue_head,queue_tail;
static HID_DEV_T *active;
static XsshHidKeys keys;
static int connected;

/* Runs in the USB transfer callback context: DO NOT call SDL or libssh2 here.
   Release/acquire atomics protect reports from a concurrent main loop. */
static void on_report(HID_DEV_T *dev,uint16_t ep,int err,uint8_t *buf,uint32_t n) {
    (void)ep;
    if(err || !buf || n<8 || dev!=active)return;
    unsigned int tail=__atomic_load_n(&queue_tail,__ATOMIC_RELAXED);
    unsigned int next=(tail+1u)&(REPORT_QUEUE-1u);
    if(next==__atomic_load_n(&queue_head,__ATOMIC_ACQUIRE))return; /* overflow */
    queue[tail].dev=dev;
    memcpy(queue[tail].report,buf,8);
    __atomic_store_n(&queue_tail,next,__ATOMIC_RELEASE);
}

void xssh_usb_keyboard_init(void) {
    active=NULL;
    connected=0;
    __atomic_store_n(&queue_head,0,__ATOMIC_RELAXED);
    __atomic_store_n(&queue_tail,0,__ATOMIC_RELAXED);
    xssh_hid_keys_reset(&keys);
    usbh_hid_init();
}

static HID_DEV_T *find_keyboard(void) {
    HID_DEV_T *p=usbh_hid_get_device_list();
    while(p) {
        if(p->iface && p->bSubClassCode==1 && p->bProtocolCode==1)
            return p; /* HID boot keyboard, not NKRO report-only or mouse */
        p=p->next;
    }
    return NULL;
}

int xssh_usb_keyboard_connected(void) {return connected;}
int xssh_software_keyboard_visible(void) {return !connected;}

void xssh_usb_keyboard_poll(uint32_t now_ms,XsshKeySink sink,void *ctx) {
    /* SDL_PollEvent will have called usbh_pooling_hubs() via joystick detect. */
    HID_DEV_T *present=find_keyboard();
    if(present != active) {
        /* The old device, if any, is already disconnected/owned by USB core.
           Ignore stale queued reports rather than dereferencing the pointer. */
        active=NULL;
        connected=0;
        __atomic_store_n(&queue_head,
                         __atomic_load_n(&queue_tail,__ATOMIC_ACQUIRE),
                         __ATOMIC_RELEASE);
        xssh_hid_keys_reset(&keys);
    }
    if(!active && present) {
        /* Protocol 0 is the standard 8-byte Boot Keyboard format. */
        if(usbh_hid_set_protocol(present,0)==HID_RET_OK) {
            active=present;
            if(usbh_hid_start_int_read(present,0,on_report)==HID_RET_OK)
                connected=1;
            else active=NULL;
        }
        /* On failed init retry only on a future frame. */
    }
    for(;;) {
        unsigned int head=__atomic_load_n(&queue_head,__ATOMIC_RELAXED);
        if(head==__atomic_load_n(&queue_tail,__ATOMIC_ACQUIRE))break;
        QueuedReport item=queue[head];
        __atomic_store_n(&queue_head,(head+1u)&(REPORT_QUEUE-1u),__ATOMIC_RELEASE);
        if(item.dev==active)xssh_hid_keys_report(&keys,item.report,now_ms,sink,ctx);
    }
    if(connected)xssh_hid_keys_tick(&keys,now_ms,sink,ctx);
}
