#ifndef FERRUM_FIRE_THERMAL_CHECKPOINT_H
#define FERRUM_FIRE_THERMAL_CHECKPOINT_H
#include <stddef.h>
#include "ferrum/fire/thermal.h"
enum {FR_THERMAL_WORLD_GENERIC=0u,FR_THERMAL_WORLD_HEAT_BENCH=1u,FR_THERMAL_WORLD_BONFIRE=2u};
/** Immutable cold checkpoint: liquid arrays follow the physical chunk order.
 * Probe contact/input is transient; only its finite inventory is persistent. */
typedef struct {
    fr_thermal_probe_t source;
    uint32_t purpose,chunk_count;
    float *water_excess;
} fr_thermal_checkpoint_t;
/** Encode validated inventories/bindings into owned versioned little-endian
 * bytes. Failure preserves both output arguments. */
bool fr_thermal_checkpoint_encode(const fr_thermal_checkpoint_t *state,const dc_chunk_t *chunks,
    uint32_t count,uint8_t **bytes,size_t *length,char *err_buf,uint32_t err_cap);
/** Decode/checksum/validate all physical bindings before replacing output. */
bool fr_thermal_checkpoint_decode(const uint8_t *bytes,size_t length,const dc_chunk_t *chunks,
    uint32_t count,fr_thermal_checkpoint_t *out,char *err_buf,uint32_t err_cap);
/** Free owned immutable liquid arrays and clear the checkpoint. */
void fr_thermal_checkpoint_release(fr_thermal_checkpoint_t *state);
#endif
