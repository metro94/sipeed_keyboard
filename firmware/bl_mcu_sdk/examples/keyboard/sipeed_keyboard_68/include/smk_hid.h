#ifndef __SMK_HID_H
#define __SMK_HID_H


#define HID_DESCRIPTOR_LEN 25
#define HID_KEYBOARD_REPORT_DESC_SIZE 63

#define HID_INT_EP          0x81
#define HID_INT_EP_SIZE     8
#define HID_INT_EP_INTERVAL 10

void smk_hid_usb_init();

#include <stdint.h>

void smk_hid_add_key(uint8_t keycode);
void smk_hid_remove_key(uint8_t keycode);

#endif