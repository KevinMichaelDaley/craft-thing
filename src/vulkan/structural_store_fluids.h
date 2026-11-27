#ifndef DC_STRUCTURAL_STORE_FLUIDS_H
#define DC_STRUCTURAL_STORE_FLUIDS_H

#include "dungeoncraft/gpu.h"
#include "ferrum/fire/bulk_phase.h"
#include "ferrum/fire/gas_checkpoint.h"

typedef struct dc_checkpoint_fluid {
    bool known;
    fr_gas_checkpoint_chunk_t *gas;
    fr_bulk_phase_chunk_t *bulk;
} dc_checkpoint_fluid_t;

/** Maximum packed bytes for one or more physical records. */
size_t dc_checkpoint_fluid_max_bytes(uint32_t count);
/** Capture fluids using the same ordered physical owners as the structure snapshot. */
bool dc_checkpoint_fluid_encode(dc_gpu_t *gpu, const dc_chunk_t *chunks,
    const uint32_t *slots, uint32_t count, const bool *gas_admitted,
    uint8_t **out, size_t *length, char *err, uint32_t cap);
/** Validate cold channel stock against the physical records before publication. */
bool dc_checkpoint_fluid_decode(const dc_chunk_t *chunks, uint32_t count,
    const uint8_t *bytes, size_t length, dc_checkpoint_fluid_t **out);
/** Release one record or the complete pending array. */
void dc_checkpoint_fluid_release(dc_checkpoint_fluid_t *record);
void dc_checkpoint_fluid_destroy(dc_checkpoint_fluid_t *records, uint32_t count);

#endif
