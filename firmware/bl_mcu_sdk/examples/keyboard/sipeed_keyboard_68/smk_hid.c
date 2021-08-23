#include "smk_hid.h"
#include "hal_usb.h"
#include "usbd_core.h"
#include "usbd_hid.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "keyboard/smk_event.h"
#include "keyboard/smk_keycode.h"

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
    0x95, 0x05, // REPORT_COUNT (5)
    0x75, 0x01, // REPORT_SIZE (1)
    0x05, 0x08, // USAGE_PAGE (LEDs)
    0x19, 0x01, // USAGE_MINIMUM (Num Lock)
    0x29, 0x05, // USAGE_MAXIMUM (Kana)
    0x91, 0x02, // OUTPUT (Data,Var,Abs)
    0x95, 0x01, // REPORT_COUNT (1)
    0x75, 0x03, // REPORT_SIZE (3)
    0x91, 0x03, // OUTPUT (Cnst,Var,Abs)
    0x05, 0x07, // USAGE_PAGE (Keyboard)
    0x15, 0x00, // LOGICAL_MINIMUM (0)
    0x25, 0x01, // LOGICAL_MAXIMUM (1)
    0x19, 0x00, // USAGE_MINIMUM (Reserved (no event indicated))
    0x29, 0x65, // USAGE_MAXIMUM (Keyboard Application)
    0x95, 0x78, // REPORT_COUNT (120)
    0x75, 0x01, // REPORT_SIZE (1)
    0x81, 0x02, // INPUT (Data,Var,Abs)
    0xc0        // END_COLLECTION
};

extern struct device *usb_fs;

static usbd_class_t hid_class;
static usbd_interface_t hid_intf;

typedef struct {
    uint8_t buf[2][16];
    uint8_t flag;
} smk_usb_hid_type;

static smk_usb_hid_type hid_usb = {
    .buf = { { 0 }, { 0 } },
    .flag = 0
};

static void smk_usb_hid_add_key(smk_usb_hid_type *hid_usb, keycode_type keycode)
{
    uint8_t *buf = hid_usb->buf[hid_usb->flag ^ 1];

    if (IS_MOD_KEYS(keycode)) {
        buf[0] |= 1U << (keycode - KC_LCTRL);
    } else {
        buf[1 + keycode / 8] |= 1U << (keycode % 8);
    }
}

static void smk_usb_hid_remove_key(smk_usb_hid_type *hid_usb, keycode_type keycode)
{
    uint8_t *buf = hid_usb->buf[hid_usb->flag ^ 1];

    if (IS_MOD_KEYS(keycode)) {
        buf[0] &= ~(1U << (keycode - KC_LCTRL));
    } else {
        buf[1 + keycode / 8] &= ~(1U << (keycode % 8));
    }
}

static void smk_usb_hid_commit(smk_usb_hid_type *hid_usb)
{
    hid_usb->flag ^= 1;
    for (uint8_t idx = 0; idx < 16; ++idx) {
        hid_usb->buf[hid_usb->flag ^ 1][idx] = hid_usb->buf[hid_usb->flag][idx];
    }
}

void usbd_hid_int_callback(uint8_t ep)
{
    usbd_ep_write(HID_INT_EP, hid_usb.buf[hid_usb.flag], HID_INT_EP_SIZE, NULL);
}
void usbd_hid_out_callback(uint8_t ep)
{
   // usbd_ep_write(HID_INT_EP, hid_usb.buf[hid_usb.flag], 8, NULL);
}
static usbd_endpoint_t hid_in_ep = {
    .ep_cb = usbd_hid_int_callback,
    .ep_addr = 0x81
};
static usbd_endpoint_t hid_out_ep = {
    .ep_cb = usbd_hid_out_callback,
    .ep_addr = 0x01
};

extern struct device *usb_dc_init(void);

void smk_hid_usb_init()
{
    usbd_hid_add_interface(&hid_class, &hid_intf);
    usbd_interface_add_endpoint(&hid_intf, &hid_in_ep);
    usbd_interface_add_endpoint(&hid_intf, &hid_out_ep);
    usbd_hid_report_descriptor_register(hid_intf.intf_num, hid_keyboard_report_desc, HID_KEYBOARD_REPORT_DESC_SIZE);
}

void smk_usb_hid_daemon_task(void *pvParameters)
{
    QueueHandle_t queue = pvParameters;
    smk_event_type event;

    HID_DEBUG("[SMK][HID] USB HID daemon start...\r\n");

    for (;;) {
        xQueueReceive(
            queue, // xQueue
            &event, // pvBuffer
            portMAX_DELAY // xTicksToWait
        );

        if (event.class != SMK_EVENT_KEYCODE) {
            continue;
        }

        switch (event.subclass) {
        case SMK_EVENT_KEYCODE_ADD:
            smk_usb_hid_add_key(&hid_usb, (keycode_type)event.data);
            HID_DEBUG("[SMK][HID] KeyCode %u add\r\n", event.data);
            break;

        case SMK_EVENT_KEYCODE_REMOVE:
            smk_usb_hid_remove_key(&hid_usb, (keycode_type)event.data);
            HID_DEBUG("[SMK][HID] KeyCode %u remove\r\n", event.data);
            break;

        case SMK_EVENT_KEYCODE_COMMIT:
            smk_usb_hid_commit(&hid_usb);
            HID_DEBUG("[SMK][HID] KeyCode commit\r\n", event.data);
            break;
        }
    }
}
