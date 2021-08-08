#include "smk_hid.h"
#include "smk_keyscan.h"
#include "hal_usb.h"
#include "usbd_core.h"
#include "usbd_hid.h"

#include "third_party/quantum_keycodes.h"

static const uint8_t hid_keyboard_report_desc[HID_KEYBOARD_REPORT_DESC_SIZE] = {
    0x05, 0x01, // USAGE_PAGE (Generic Desktop)
    0x09, 0x06, // USAGE (Keyboard)
    0xa1, 0x01, // COLLECTION (Application)
    0x05, 0x07, // USAGE_PAGE (Keyboard)
    0x19, 0xe0, // USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xe7, // USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00, // LOGICAL_MINIMUM (0)
    0x25, 0x01, // LOGICAL_MAXIMUM (1)
    0x75, 0x01, // REPORT_SIZE (1)
    0x95, 0x08, // REPORT_COUNT (8)
    0x81, 0x02, // INPUT (Data,Var,Abs)
    0x95, 0x01, // REPORT_COUNT (1)
    0x75, 0x08, // REPORT_SIZE (8)
    0x81, 0x03, // INPUT (Cnst,Var,Abs)
    0x95, 0x05, // REPORT_COUNT (5)
    0x75, 0x01, // REPORT_SIZE (1)
    0x05, 0x08, // USAGE_PAGE (LEDs)
    0x19, 0x01, // USAGE_MINIMUM (Num Lock)
    0x29, 0x05, // USAGE_MAXIMUM (Kana)
    0x91, 0x02, // OUTPUT (Data,Var,Abs)
    0x95, 0x01, // REPORT_COUNT (1)
    0x75, 0x03, // REPORT_SIZE (3)
    0x91, 0x03, // OUTPUT (Cnst,Var,Abs)
    0x95, 0x06, // REPORT_COUNT (6)
    0x75, 0x08, // REPORT_SIZE (8)
    0x15, 0x00, // LOGICAL_MINIMUM (0)
    0x25, 0xFF, // LOGICAL_MAXIMUM (255)
    0x05, 0x07, // USAGE_PAGE (Keyboard)
    0x19, 0x00, // USAGE_MINIMUM (Reserved (no event indicated))
    0x29, 0x65, // USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00, // INPUT (Data,Ary,Abs)
    0xc0        // END_COLLECTION
};

extern struct device *usb_fs;

static usbd_class_t hid_class;
static usbd_interface_t hid_intf;

static uint8_t flag = 0;
static uint8_t sendbuffer[2][8] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};

void usbd_hid_int_callback(uint8_t ep)
{
    usbd_ep_write(HID_INT_EP, sendbuffer[flag], 8, NULL);
    //MSG("A\r\n");
}

static usbd_endpoint_t hid_in_ep = {
    .ep_cb = usbd_hid_int_callback,
    .ep_addr = 0x81
};

extern struct device *usb_dc_init(void);

void smk_hid_usb_init()
{
    usbd_hid_add_interface(&hid_class, &hid_intf);
    usbd_interface_add_endpoint(&hid_intf, &hid_in_ep);
    usbd_hid_report_descriptor_register(0, hid_keyboard_report_desc, HID_KEYBOARD_REPORT_DESC_SIZE);
}

void smk_hid_add_key(uint8_t keycode)
{
    if (IS_MOD(keycode)) {
        sendbuffer[flag][0] |= 1U << (keycode - KC_LCTRL);
    } else {
        uint8_t *buf = sendbuffer[flag ^ 1U];

        if (sendbuffer[flag][7] != 0x00U) {
            return;
        }

        for (size_t i = 2; i < 7; ++i) {
            if (sendbuffer[flag][i] == keycode) {
                return;
            } else if (sendbuffer[flag][i] > keycode || sendbuffer[flag][i] == 0x00U) {
                memcpy(buf, sendbuffer[flag], i);
                buf[i] = keycode;
                memcpy(buf + i + 1, sendbuffer[flag] + i, 8 - i - 1);
                flag ^= 1U;
                return;
            }
        }

        buf[7] = keycode;
    }
}

void smk_hid_remove_key(uint8_t keycode)
{
    if (IS_MOD(keycode)) {
        sendbuffer[flag][0] &= ~(1U << (keycode - KC_LCTRL));
    } else {
        uint8_t *buf = sendbuffer[flag ^ 1];

        for (size_t i = 2; i < 8; ++i) {
            if (sendbuffer[flag][i] == keycode) {
                memcpy(buf, sendbuffer[flag], i);
                memcpy(buf + i, sendbuffer[flag] + i + 1, 8 - i - 1);
                buf[7] = 0x00;
                flag ^= 1U;
                break;
            }
        }
    }
}
