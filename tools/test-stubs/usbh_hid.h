#ifndef USBH_HID_H
#define USBH_HID_H
#include <stdint.h>
#define HID_RET_OK 0
struct usbhid_dev;
typedef void (HID_IR_FUNC)(struct usbhid_dev*,uint16_t,int,uint8_t*,uint32_t);
typedef struct usbhid_dev {
    void *iface;
    uint8_t bSubClassCode,bProtocolCode;
    struct usbhid_dev *next;
} HID_DEV_T;
#endif
