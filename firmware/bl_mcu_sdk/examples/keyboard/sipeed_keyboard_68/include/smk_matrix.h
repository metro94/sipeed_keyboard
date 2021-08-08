#ifndef __SMK_MATRIX_H
#define __SMK_MATRIX_H

#include <stdint.h>
#include <errno.h>

#include "FreeRTOS.h"
#include "queue.h"

#include "bflb_platform.h"
#define MATRIX_DBG(fmt, ...) MSG_DBG(fmt, ##__VA_ARGS__)

typedef uint8_t smk_matrix_gpio_t;

typedef struct {
    uint8_t key_num;
    uint8_t col_num;
    uint8_t row_num;
    const smk_matrix_gpio_t *col_gpio;
    const smk_matrix_gpio_t *row_gpio;
    const uint8_t *key_avail;
    uint32_t max_jitter_ms;
} smk_matrix_hardware_t;

typedef struct {
    uint8_t *key_raw;
    uint8_t *key_next;
    uint8_t *key_last;
    TickType_t max_jitter_ticks;
    TickType_t tick_raw_changed;
    uint8_t next_updated;
    uint8_t jitter_detected;
} smk_matrix_keyscan_t;

typedef uint16_t smk_matrix_keycode_t;

typedef struct {
    const smk_matrix_keycode_t *layer[2];
    uint8_t cur_layer;
} smk_matrix_keymap_t;

#define EVENT_TYPE_KEYUP   0U
#define EVENT_TYPE_KEYDOWN 1U
#define EVENT_TYPE_TICK    0xFFU

typedef struct {
    TickType_t tick;
    uint8_t type;
    uint8_t pos;
} smk_matrix_keyscan_event_t;

typedef struct {
    const smk_matrix_hardware_t *hardware;
    smk_matrix_keyscan_t scan;
    smk_matrix_keymap_t map;

    xQueueHandle queue_scan_event;
    // xQueueHandle queue_map_event;
} smk_matrix_t;

int smk_matrix_init(smk_matrix_t *matrix);
int smk_matrix_keyscan_init(smk_matrix_t *matrix);
int smk_matrix_keymap_init(smk_matrix_t *matrix);

void smk_matrix_keyscan_task(void *pvParameters);
void smk_matrix_keymap_task(void *pvParameters);

#endif
