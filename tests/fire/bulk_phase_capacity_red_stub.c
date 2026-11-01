#include "ferrum/fire/bulk_phase.h"
bool fr_bulk_phase_set_capacity(const fr_bulk_cell_t *source,uint32_t capacity_fp,
    const float density[FR_BULK_PHASES],fr_bulk_cell_t *out) {
    (void)source;(void)capacity_fp;(void)density;(void)out;return false;
}
