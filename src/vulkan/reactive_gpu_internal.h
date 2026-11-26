#ifndef FR_REACTIVE_GPU_INTERNAL_H
#define FR_REACTIVE_GPU_INTERNAL_H
#include "body_address_gpu.h"
#include "ferrum/fire/gpu.h"

static inline bool fr_gpu_reactive_has_coating(const fr_reactive_state_t *state) {
    return state->coating_oil_fuel_fp||state->coating_jelly_fuel_fp||
        state->coating_oxidizer_fp||state->coating_remainders;
}

/** Publish the lazy root into a newly allocated body work header. */
void fr_gpu_reactive_bind_work(dc_gpu_t *gpu,uint32_t *header);
/** Release reactive resources after the device is idle. */
void fr_gpu_reactive_destroy(dc_gpu_t *gpu);
/** Preflight appearance-like handle banks before geometry layout publication. */
bool fr_gpu_reactive_reserve_shapes(dc_gpu_t *gpu,uint32_t words,char *err,uint32_t cap);
/** Retire the old CELL/PARTICLE state before a validated cold physical upload.
 * Resource failure leaves both physical and reactive source data unchanged. */
bool fr_gpu_reactive_clear_chunk(dc_gpu_t *gpu,uint32_t slot,char *err,uint32_t cap);
/** Cold birth of a complete authored pixel body's reactive parcels. Records
 * have fresh consecutive IDs, in increasing body rest-cell order. Revalidate
 * shape and ownership before publishing the entire arena batch. */
bool fr_gpu_reactive_attach_authored_body(dc_gpu_t *gpu,uint32_t slot,
    const fr_reactive_record_t *records,uint32_t count,char *err,uint32_t cap);
#endif
