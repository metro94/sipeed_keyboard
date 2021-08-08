#include "smk_keymap.h"

#include "FreeRTOS.h"
#include "queue.h"

#include "third_party/quantum_keycodes.h"
#include "smk_hid.h"

#if 0
#define KEYMAP_TOTAL_LAYERS 1

static const smk_matrix_keycode_t smk_matrix_keymap[KEYMAP_TOTAL_LAYERS][20] = {
    {
        XXXXXXX, KC_P1,   KC_P4,   KC_P7,   KC_NLCK,
        KC_P0,   KC_P2,   KC_P5,   KC_P8,   KC_PSLS,
        KC_PDOT, KC_P3,   KC_P6,   KC_P9,   KC_PAST,
        KC_PENT, XXXXXXX, KC_PPLS, XXXXXXX, KC_PMNS
    }
};
#elif 1
#define KEYMAP_TOTAL_LAYERS 2

static const smk_matrix_keycode_t smk_matrix_keymap[KEYMAP_TOTAL_LAYERS][77] = {
    {
        KC_ESC,  KC_TAB,  KC_CAPS, KC_LSFT, KC_LCTL, XXXXXXX, XXXXXXX,
        KC_1,    KC_Q,    KC_A,    KC_Z,    KC_LGUI, KC_DOWN, KC_RGHT,
        KC_2,    KC_W,    KC_S,    KC_X,    KC_LALT, KC_RCTL, KC_LEFT,
        KC_3,    KC_E,    KC_D,    KC_C,    XXXXXXX, KC_UP,   KC_PGDN,
        KC_4,    KC_R,    KC_F,    KC_V,    XXXXXXX, KC_RSFT, XXXXXXX,
        KC_5,    KC_T,    KC_G,    KC_B,    KC_SPC,  XXXXXXX, KC_PGUP,
        KC_6,    KC_Y,    KC_H,    KC_N,    XXXXXXX, KC_QUOT, KC_ENT ,
        KC_7,    KC_U,    KC_J,    KC_M,    XXXXXXX, KC_BSLS, KC_DEL ,
        KC_8,    KC_I,    KC_K,    KC_COMM, XXXXXXX, KC_LBRC, KC_RBRC,
        KC_9,    KC_O,    KC_L,    KC_DOT,  KC_RALT, KC_BSPC, KC_GRV ,
        KC_0,    KC_P,    KC_SCLN, KC_SLSH, MO(1),   KC_MINS, KC_EQL 
    }, {
        _______, _______, _______, _______, _______, _______, _______,
        KC_F1,   _______, _______, _______, _______, _______, _______,
        KC_F2,   _______, _______, _______, _______, _______, _______,
        KC_F3,   _______, _______, _______, _______, _______, _______,
        KC_F4,   _______, _______, _______, _______, _______, _______,
        KC_F5,   _______, _______, _______, _______, _______, _______,
        KC_F6,   _______, _______, _______, _______, _______, _______,
        KC_F7,   _______, _______, _______, _______, _______, _______,
        KC_F8,   _______, _______, _______, _______, _______, _______,
        KC_F9,   _______, _______, _______, _______, _______, _______,
        KC_F10,  _______, _______, _______, _______, KC_F11,  KC_F12 
    }
};
#else
#define KEYMAP_TOTAL_LAYERS 2

static const smk_matrix_keycode_t smk_matrix_keymap[KEYMAP_TOTAL_LAYERS][20] = {
    {
        XXXXXXX, KC_1,    KC_4,    KC_7,    KC_LCTL,
        KC_0,    KC_2,    KC_5,    KC_8,    KC_LSFT,
        KC_A,    KC_3,    KC_6,    KC_9,    KC_LALT,
        MO(1),   XXXXXXX, KC_PSCR, XXXXXXX, KC_LGUI
    }, {
        _______, KC_F1,   KC_F4,   KC_F7,   _______,
        KC_ESC,  KC_F2,   KC_F5,   KC_F8,   _______,
        _______, KC_F3,   KC_F6,   KC_F9,   _______,
        MO(1),   _______, _______, _______, _______
    }
};
#endif

static inline smk_matrix_keycode_t search_keycode(smk_matrix_t *matrix, uint8_t layer_id, uint8_t key_pos)
{
    smk_matrix_keymap_t *map = &matrix->map;

    do {
        smk_matrix_keycode_t keycode = map->layer[layer_id][key_pos];
        if (keycode != KC_TRANSPARENT) {
            return keycode;
        }
    } while (layer_id-- != 0);

    return KC_NO;
}

int smk_matrix_keymap_init(smk_matrix_t *matrix)
{
    smk_matrix_keymap_t *map = &matrix->map;
    for (uint8_t id = 0; id < KEYMAP_TOTAL_LAYERS; ++id) {
        map->layer[id] = smk_matrix_keymap[id];
    }
    map->cur_layer = 0;

    return 0;
}

void smk_matrix_handle_event(smk_matrix_t *matrix, const smk_matrix_keyscan_event_t *event)
{
    smk_matrix_keymap_t *map = &matrix->map;

    smk_matrix_keycode_t keycode = search_keycode(matrix, map->cur_layer, event->pos);

    switch (keycode & 0xFF00U) {
    case QK_BASIC:
        switch (event->type) {
        case EVENT_TYPE_KEYDOWN:
            MATRIX_DBG("[SMK] KeyCode %u add @%u\r\n", keycode & 0xFF, event->tick);
            smk_hid_add_key((uint8_t)(keycode & 0xFFU));
            break;

        case EVENT_TYPE_KEYUP:
            // TODO: need elegant implementation
            for (int8_t id = KEYMAP_TOTAL_LAYERS - 1; id >= 0; --id) {
                smk_matrix_keycode_t keycode = search_keycode(matrix, id, event->pos);
                if ((keycode & 0xFF00U) == QK_BASIC) {
                    MATRIX_DBG("[SMK] KeyCode %u remove @%u\r\n", keycode & 0xFF, event->tick);
                    smk_hid_remove_key((uint8_t)(keycode & 0xFFU));
                }
            }
            break;
        }
        break;

    case QK_MOMENTARY:
        switch (event->type) {
        case EVENT_TYPE_KEYDOWN:
            MATRIX_DBG("[SMK] KeyMap change to layer %u @%u\r\n", keycode & 0xFF, event->tick);
            map->cur_layer = keycode & 0xFF;
            break;

        case EVENT_TYPE_KEYUP:
            MATRIX_DBG("[SMK] KeyMap change to layer %u @%u\r\n", 0, event->tick);
            map->cur_layer = 0;
            break;
        }
        break;
    }
}

void smk_matrix_keymap_task(void *pvParameters)
{
    smk_matrix_t *matrix = (smk_matrix_t *)pvParameters;

    smk_matrix_keyscan_event_t event;

    for (;;) {
        if (xQueueReceive(matrix->queue_scan_event, &event, portMAX_DELAY) == errQUEUE_EMPTY) {
            continue;
        }

        if (event.type != EVENT_TYPE_TICK) {
            MATRIX_DBG("[SMK] KeyPos %u %s @%u\r\n", event.pos, event.type == EVENT_TYPE_KEYDOWN ? "down" : "up", (uint32_t)event.tick);
        }
        
        smk_matrix_handle_event(matrix, &event);
    }
}
