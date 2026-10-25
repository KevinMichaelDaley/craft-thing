#ifndef FR_GAS_GPU_INTERNAL_H
#define FR_GAS_GPU_INTERNAL_H
#include "gpu_internal.h"
/** Destroy an optional gas module after its submitted work completes. */
void fr_gas_gpu_destroy(dc_gpu_t *gpu);
/** Invalidate derived flat caches when atlas contents or page coordinates change. */
void fr_gas_gpu_invalidate_cache(dc_gpu_t *gpu);
/** Composite read-only gas pigment; presentation never advances gas. */
void fr_gas_record_overlay(dc_gpu_t *gpu);
#endif
