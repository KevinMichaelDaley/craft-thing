#ifndef FERRUM_FIRE_BULK_PHASE_H
#define FERRUM_FIRE_BULK_PHASE_H

#include <stdbool.h>
#include <stdint.h>

enum { FR_BULK_WATER, FR_BULK_OIL, FR_BULK_SLIME, FR_BULK_PHASES };
#define FR_BULK_CELL_FULL_FP 65536u

/** Continuous Q16 phase inventory. Retained volume is finite stock held when
 * geometry has less free space than the occupied phases; it is not occupancy. */
typedef struct {
    uint32_t volume_fp[FR_BULK_PHASES];
    uint32_t retained_fp[FR_BULK_PHASES];
    uint32_t capacity_fp;
} fr_bulk_cell_t;

/** Transfer the admitted portion of one phase to a neighboring cell while
 * retaining any source stock that cannot fit in shared destination capacity. */
bool fr_bulk_phase_transfer(const fr_bulk_cell_t *source,
    const fr_bulk_cell_t *destination,uint32_t phase,uint32_t requested_fp,
    fr_bulk_cell_t *source_out,fr_bulk_cell_t *destination_out,
    uint32_t *moved_fp);

/** Order liquid phases from the top surface to the bottom by profile density.
 * Equal densities use phase identity as a deterministic tie break. */
bool fr_bulk_phase_density_order(const float density[FR_BULK_PHASES],
    uint32_t order[FR_BULK_PHASES]);

#endif
