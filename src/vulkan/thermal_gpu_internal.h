#ifndef FR_THERMAL_GPU_INTERNAL_H
#define FR_THERMAL_GPU_INTERNAL_H
#include "gpu_internal.h"
#include "ferrum/fire/thermal_checkpoint.h"
struct fr_reactive_gpu;
/** Record one accepted physical interval; no work or allocation when disabled. */
void fr_thermal_record_step(dc_gpu_t *gpu,float seconds);
/** Refresh and render thermal feedback without changing physical state. */
void fr_thermal_record_overlay(dc_gpu_t *gpu);
/** Release thermal resources before their reactive backing storage. */
void fr_thermal_gpu_destroy(dc_gpu_t *gpu);
/** Cold physical-slot replacement after validation and completed upload. */
void fr_thermal_clear_water_chunk(dc_gpu_t *gpu,uint32_t slot);
/** Cold validated gas replacement invalidates its derived emission cache. */
bool fr_combustion_clear_emission_chunk(dc_gpu_t *gpu,uint32_t slot,char *err,uint32_t cap);
/** Reserve resources/unique identities before wood/stone brush mutation. */
bool fr_thermal_prepare_paint(dc_gpu_t *gpu,uint32_t x,uint32_t y,uint32_t radius,
    uint32_t material,char *err,uint32_t cap);
/** Collect immutable finite-source and logical liquid inventories at a cold boundary. */
bool fr_thermal_checkpoint_collect(dc_gpu_t *gpu,const dc_chunk_t *chunks,uint32_t count,
    fr_thermal_checkpoint_t *out,char *err,uint32_t cap);
/** Prepare detached thermal resources against the prepared parcel root. */
bool fr_thermal_checkpoint_prepare(dc_gpu_t *gpu,struct fr_reactive_gpu *reactive,
    const fr_thermal_checkpoint_t *state,const dc_chunk_t *chunks,
    struct fr_thermal_gpu **out,char *err,uint32_t cap);
/** Publish only after every checkpoint resource and binding is validated. */
void fr_thermal_checkpoint_publish(dc_gpu_t *gpu,struct fr_thermal_gpu *prepared);
/** Release an unpublished transaction without changing live control addresses. */
void fr_thermal_checkpoint_discard(dc_gpu_t *gpu,struct fr_thermal_gpu *prepared);
/** Preflight and bind one immutable pending liquid channel before/after physical upload. */
bool fr_thermal_checkpoint_preflight_chunk(dc_gpu_t *gpu,uint32_t index,char *err,uint32_t cap);
bool fr_thermal_checkpoint_bind_chunk(dc_gpu_t *gpu,uint32_t slot,uint32_t index,char *err,uint32_t cap);
/** Remove a consumed pending packet in lockstep with the physical checkpoint. */
void fr_thermal_checkpoint_remove_pending(dc_gpu_t *gpu,uint32_t index,uint32_t last);
#endif
