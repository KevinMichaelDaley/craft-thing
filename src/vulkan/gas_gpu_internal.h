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
/** Read completed CFL scalars and reduction width; cold diagnostics only. */
bool fr_gas_gpu_rate_metadata(const dc_gpu_t *gpu,float rates[3],uint32_t *width);
/** Read one completed source activity flag for cold tests; never runtime scheduling. */
bool fr_gas_gpu_source_tile_awake(const dc_gpu_t *gpu,uint32_t x,uint32_t y,bool *awake);
/** Read the completed GPU activity-route marker; cold diagnostics only. */
bool fr_gas_gpu_activity_metadata(const dc_gpu_t *gpu,uint32_t *mode);
/** Read configured hybrid line workgroup width for cold diagnostics. */
bool fr_gas_gpu_hybrid_line_group(const dc_gpu_t *gpu,uint32_t *width);
/** Read the completed GPU zero-energy policy marker; cold diagnostics only. */
bool fr_gas_gpu_zero_energy_metadata(const dc_gpu_t *gpu,bool *selected);
/** Composite read-only gas pigment; presentation never advances gas. */
void fr_gas_record_overlay(dc_gpu_t *gpu);
#endif
