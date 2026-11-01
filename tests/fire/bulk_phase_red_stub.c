#include "ferrum/fire/bulk_phase.h"
bool fr_bulk_phase_transfer(const fr_bulk_cell_t *source,
    const fr_bulk_cell_t *destination,uint32_t phase,uint32_t requested_fp,
    fr_bulk_cell_t *source_out,fr_bulk_cell_t *destination_out,uint32_t *moved_fp) {
    (void)source;(void)destination;(void)phase;(void)requested_fp;
    (void)source_out;(void)destination_out;(void)moved_fp;return false;
}
bool fr_bulk_phase_density_order(const float density[FR_BULK_PHASES],
    uint32_t order[FR_BULK_PHASES]) {(void)density;(void)order;return false;}
