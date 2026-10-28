#ifndef USBH_LIB_H
#define USBH_LIB_H
#include "usbh_hid.h"
extern void usbh_hid_init(void);
extern HID_DEV_T *usbh_hid_get_device_list(void);
extern int usbh_hid_set_protocol(HID_DEV_T*,uint8_t);
extern int usbh_hid_start_int_read(HID_DEV_T*,uint8_t,HID_IR_FUNC*);
#endif
