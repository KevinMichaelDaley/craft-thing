#ifndef FR_GAS_GPU_INTERNAL_H
#define FR_GAS_GPU_INTERNAL_H
#include "gpu_internal.h"
/** Destroy an optional gas module after its submitted work completes. */
void fr_gas_gpu_destroy(dc_gpu_t *gpu);
/** Invalidate derived flat caches when atlas contents or page coordinates change. */
void fr_gas_gpu_invalidate_cache(dc_gpu_t *gpu);
/** Submit one gas command buffer and wait for its completion fence only. */
bool fr_gas_submit(dc_gpu_t *gpu);
/** Read completed scalar pressure dispatch size; diagnostic only, no field scan. */
bool fr_gas_gpu_pressure_tiles(const dc_gpu_t *gpu,uint32_t *tiles);
/** Composite read-only gas pigment; presentation never advances gas. */
void fr_gas_record_overlay(dc_gpu_t *gpu);
#endif
