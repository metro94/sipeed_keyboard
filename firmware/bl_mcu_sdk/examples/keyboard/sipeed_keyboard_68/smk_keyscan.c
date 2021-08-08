#include "smk_keyscan.h"

#include <string.h>
#include "FreeRTOS.h"
#include "timers.h"

#include "hal_gpio.h"

static inline void set_gpio_input_high(uint32_t pin)
{
    gpio_set_mode(pin, GPIO_INPUT_PP_MODE);
}

static inline void set_gpio_output(uint32_t pin, uint32_t value)
{
    gpio_write(pin, value);
    gpio_set_mode(pin, GPIO_OUTPUT_MODE);
}

static inline uint8_t is_bool_array_diff(uint8_t *src, uint8_t *dst, size_t num)
{
    uint8_t ret = 0U;

    while (num--) {
        ret |= (*src++) ^ (*dst++);
    }

    return !!ret;
}

static inline uint8_t is_bool_element_diff(uint8_t *src, uint8_t *dst, size_t id)
{
    return ((src[id / 8] ^ dst[id / 8]) >> (id % 8)) & 0x1U;
}

static inline uint8_t get_bool_element(uint8_t *arr, size_t id)
{
    return (arr[id / 8] >> (id % 8)) & 0x1U;
}

static inline void update_bool_element(uint8_t *src, uint8_t *dst, size_t id)
{
    uint8_t mask = 1U << (id % 8);
    dst[id / 8] = (dst[id / 8] & ~mask) | (src[id / 8] & mask);
}

static inline void select_col(smk_matrix_t *matrix, uint8_t col)
{
    set_gpio_output(matrix->hardware->col_gpio[col], 0U);
}

static inline void unselect_col(smk_matrix_t *matrix, uint8_t col)
{
    set_gpio_output(matrix->hardware->col_gpio[col], 1U);
    set_gpio_input_high(matrix->hardware->col_gpio[col]);
}

static inline void select_col_delay(void)
{
    bflb_platform_delay_us(1);
}

static inline void unselect_col_delay(void)
{
    bflb_platform_delay_us(5);
}

static inline uint32_t read_row(smk_matrix_t *matrix, uint8_t row)
{
    return (uint32_t)gpio_read(matrix->hardware->row_gpio[row]);
}

int smk_matrix_keyscan_init(smk_matrix_t *matrix)
{
    const smk_matrix_hardware_t *hw = matrix->hardware;
    smk_matrix_keyscan_t *scan = &matrix->scan;

    size_t key_size = (hw->key_num + 7) / 8;

    scan->key_raw  = pvPortMalloc(key_size);
    scan->key_next = pvPortMalloc(key_size);
    scan->key_last = pvPortMalloc(key_size);

    memset(scan->key_raw,  0, key_size);
    memset(scan->key_next, 0, key_size);
    memset(scan->key_last, 0, key_size);

    scan->max_jitter_ticks = pdMS_TO_TICKS(hw->max_jitter_ms);
    scan->tick_raw_changed = (TickType_t)0;
    scan->jitter_detected = 0U;
    scan->next_updated = 0U;

    return 0;
}

void smk_matrix_init_gpio(smk_matrix_t *matrix)
{
    const smk_matrix_hardware_t *hw = matrix->hardware;

    for (uint8_t col = 0; col < hw->col_num; ++col) {
        set_gpio_input_high(hw->col_gpio[col]);
    }

    for (uint8_t row = 0; row < hw->row_num; ++row) {
        set_gpio_input_high(hw->row_gpio[row]);
    }
}

void smk_matrix_read_gpio(smk_matrix_t *matrix)
{
    const smk_matrix_hardware_t *hw = matrix->hardware;
    uint32_t key_size = (hw->key_num + 7) / 8;
    
    smk_matrix_keyscan_t *scan = &matrix->scan;
    uint8_t *buf = scan->key_raw;

    memset(buf, 0, key_size);

    uint8_t key_pos = 0;
    for (uint8_t col = 0; col < hw->col_num; ++col) {
        select_col(matrix, col);
        select_col_delay();

        for (uint8_t row = 0; row < hw->row_num; ++row) {
            if (read_row(matrix, row) == 0) {
                // keypress detected
                buf[key_pos / 8] |= 1U << (key_pos % 8);
            }
            ++key_pos;
        }

        unselect_col(matrix, col);
        unselect_col_delay();
    }
}

void smk_matrix_debounce(smk_matrix_t *matrix)
{
    const smk_matrix_hardware_t *hw = matrix->hardware;
    size_t key_size = (hw->key_num + 7) / 8;

    smk_matrix_keyscan_t *scan = &matrix->scan;
    uint8_t *raw  = scan->key_raw;
    uint8_t *next = scan->key_next;

    TickType_t cur_tick = xTaskGetTickCount();

    uint8_t raw_changed = is_bool_array_diff(scan->key_raw, scan->key_next, key_size);

    if (cur_tick - scan->tick_raw_changed <= scan->max_jitter_ticks) {
        // In debouncing state, try to wait until stable
        if (raw_changed) {
            // Matrix is jittering, update last jitter tick
            scan->tick_raw_changed = cur_tick;
            memcpy(next, raw, key_size);
        }
    } else {
        // Not in debouncing state, try to detect debouncing
        if (raw_changed) {
            // Jitter is detected, update last jitter tick
            scan->jitter_detected = 1U;
            scan->tick_raw_changed = cur_tick;
            memcpy(next, raw, key_size);
        } else if (scan->jitter_detected) {
            // Clear last debouncing state and commit to next state
            scan->jitter_detected = 0U;
            scan->next_updated = 1U;
        }
    }
}

void smk_matrix_trigger_keyscan_event(smk_matrix_t *matrix)
{
    const smk_matrix_hardware_t *hw = matrix->hardware;

    smk_matrix_keyscan_t *scan = &matrix->scan;
    uint8_t *next = scan->key_next;
    uint8_t *last = scan->key_last;
    
    TickType_t cur_tick = xTaskGetTickCount();

    if (scan->next_updated) {
        for (uint8_t pos = 0; pos < hw->key_num; ++pos) {
            if (is_bool_element_diff(next, last, pos)) {
                smk_matrix_keyscan_event_t event = {
                    .tick = cur_tick,
                    .type = get_bool_element(next, pos) ? EVENT_TYPE_KEYDOWN : EVENT_TYPE_KEYUP,
                    .pos  = pos
                };

                if (xQueueSend(matrix->queue_scan_event, &event, 0) == errQUEUE_FULL) {
                    // KeyScan Event FIFO is full now, send this event next time
                    return;
                }

                update_bool_element(next, last, pos);
            }
        }
    }
    
    {
        // Send periodic heartbeat event
        smk_matrix_keyscan_event_t event = {
            .tick = cur_tick,
            .type = EVENT_TYPE_TICK,
            .pos  = 0U
        };

        xQueueSend(matrix->queue_scan_event, &event, 0);
    }

    scan->next_updated = 0U;
}

static void keyscan_timer_handler(TimerHandle_t xTimer)
{
    TaskHandle_t task = (TaskHandle_t)pvTimerGetTimerID(xTimer);
    xTaskNotifyGive(task);
}

void smk_matrix_keyscan_task(void *pvParameters)
{
    smk_matrix_t         *matrix  = (smk_matrix_t *)pvParameters;
    smk_matrix_keyscan_t *scan    = &matrix->scan;

    // Init last update tick for debouncing
    
    scan->tick_raw_changed = xTaskGetTickCount();

    // Set heartbeat timer to notify current task periodically

    TimerHandle_t beat = xTimerCreate(
        "KeyScan Task Beat",
        pdMS_TO_TICKS(4),
        pdTRUE,
        xTaskGetCurrentTaskHandle(),
        keyscan_timer_handler
    );
    xTimerStart(beat, 0);

    // Start handling loop

    for (;;) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == 0U) {
            continue;
        }

        // MATRIX_DBG("[SMK] KeyScan task is awake!\r\n");

        smk_matrix_read_gpio(matrix);
        smk_matrix_debounce(matrix);
        smk_matrix_trigger_keyscan_event(matrix);
    }
}
