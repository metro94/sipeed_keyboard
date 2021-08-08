#include "smk_matrix.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#if 1
const uint8_t smk_matrix_keyscan_fifo_depth = 16;
const smk_matrix_gpio_t smk_matrix_col_gpio[] = { 20, 21, 22, 23, 24, 25, 26, 27, 29, 30, 31 };
const smk_matrix_gpio_t smk_matrix_row_gpio[] = { 0, 1, 2, 3, 4, 5, 6 };
const uint8_t smk_matrix_key_avail[] = {
    0x9F, 0xFF, 0xFF, 0xFD, 0xFA, 0xBE, 0xDF, 0xEF, 0xFF, 0x1F
};

const smk_matrix_hardware_t smk_matrix_hardware = {
    .key_num       = 11 * 7,
    .col_num       = 11,
    .row_num       = 7,
    .col_gpio      = smk_matrix_col_gpio,
    .row_gpio      = smk_matrix_row_gpio,
    .key_avail     = smk_matrix_key_avail,
    .max_jitter_ms = 20
};
#else
const uint8_t smk_matrix_keyscan_fifo_depth = 16;
const smk_matrix_gpio_t smk_matrix_col_gpio[] = { 0, 1, 2, 23 };
const smk_matrix_gpio_t smk_matrix_row_gpio[] = { 24, 25, 26, 27, 9 };
const uint8_t smk_matrix_key_avail[] = {
    0xFF, 0xFF, 0x0F
};

const smk_matrix_hardware_t smk_matrix_hardware = {
    .key_num       = 4 * 5,
    .col_num       = 4,
    .row_num       = 5,
    .col_gpio      = smk_matrix_col_gpio,
    .row_gpio      = smk_matrix_row_gpio,
    .key_avail     = smk_matrix_key_avail,
    .max_jitter_ms = 20
};
#endif

const uint16_t smk_matrix_keyscan_task_stack_depth = 1024;
const UBaseType_t smk_matrix_keyscan_task_priority = 14;
const uint16_t smk_matrix_keymap_task_stack_depth  = 1024;
const UBaseType_t smk_matrix_keymap_task_priority  = 15;

int smk_matrix_init(smk_matrix_t *matrix)
{
    int ret;

    matrix->hardware = &smk_matrix_hardware;
    MATRIX_DBG(
        "[SMK] keyboard matrix: %ux%u\r\n",
        matrix->hardware->col_num, matrix->hardware->row_num
    );

    ret = smk_matrix_keyscan_init(matrix);
    if (ret != 0) {
        return ret;
    }

    ret = smk_matrix_keymap_init(matrix);
    if (ret != 0) {
        return ret;
    }

    xTaskCreate(
        smk_matrix_keyscan_task,
        "KeyScan Task",
        smk_matrix_keyscan_task_stack_depth,
        matrix,
        smk_matrix_keyscan_task_priority,
        NULL
    );

    xTaskCreate(
        smk_matrix_keymap_task,
        "KeyMap Task",
        smk_matrix_keymap_task_stack_depth,
        matrix,
        smk_matrix_keymap_task_priority,
        NULL
    );

    matrix->queue_scan_event = xQueueCreate(
        smk_matrix_keyscan_fifo_depth,
        sizeof(smk_matrix_keyscan_event_t)
    );

    MATRIX_DBG("[SMK] keyboard matrix: init done!\r\n");

    return 0;
}
