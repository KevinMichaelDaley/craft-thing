#ifndef FERRUM_FIRE_BULK_PHASE_H
#define FERRUM_FIRE_BULK_PHASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dungeoncraft/chunk.h"

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

/** Apply a moving solid's new free-space capacity. Displaced volume remains in
 * retained inventory; newly exposed space restores retained volume by density. */
bool fr_bulk_phase_set_capacity(const fr_bulk_cell_t *source,uint32_t capacity_fp,
    const float density[FR_BULK_PHASES],fr_bulk_cell_t *out);

/** Cold chunk channel. Water remains authoritative in dc_cell_t.fluid_mass;
 * retained arrays mark the part of each phase stock blocked by solid occupancy. */
typedef struct {
    uint32_t oil_fp[DC_CHUNK_CELLS],slime_fp[DC_CHUNK_CELLS];
    uint32_t oil_retained_fp[DC_CHUNK_CELLS],slime_retained_fp[DC_CHUNK_CELLS];
    /** Per-phase thermal energy relative to the ambient-temperature baseline. */
    float oil_excess[DC_CHUNK_CELLS],slime_excess[DC_CHUNK_CELLS];
} fr_bulk_phase_chunk_t;

/** Size of the fixed versioned chunk packet. */
size_t fr_bulk_phase_chunk_bytes(void);
/** Encode a validated chunk packet with a checksum. */
bool fr_bulk_phase_chunk_encode(const dc_chunk_t *chunk,
    const fr_bulk_phase_chunk_t *phase,uint8_t *bytes,size_t capacity);
/** Decode transactionally; malformed or overfull data leaves output unchanged. */
bool fr_bulk_phase_chunk_decode(const dc_chunk_t *chunk,const uint8_t *bytes,
    size_t length,fr_bulk_phase_chunk_t *out);

#endif
