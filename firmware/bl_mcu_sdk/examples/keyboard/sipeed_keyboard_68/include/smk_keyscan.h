#ifndef __SMK_KEYSCAN_H
#define __SMK_KEYSCAN_H

#include "smk_matrix.h"

void smk_matrix_init_gpio(smk_matrix_t *matrix);
void smk_matrix_read_gpio(smk_matrix_t *matrix);
void smk_matrix_debounce(smk_matrix_t *matrix);
void smk_matrix_trigger_keyscan_event(smk_matrix_t *matrix);

#endif