#ifndef FERRUM_ASSETS_PROP_GPU_H
#define FERRUM_ASSETS_PROP_GPU_H

#include "dungeoncraft/gpu.h"
#include "ferrum/assets/prop.h"

/** Place a preloaded terrain-only frame at resident simulation coordinates.
 * GPU preflight rejects unknown cells, body overlaps and unsupported roles
 * before publishing any physical or appearance pixel. Absent pixels preserve
 * the world. This explicit authoring boundary may wait for completion; ordinary
 * frames never parse assets or read placement diagnostics.
 */
bool fr_gpu_place_terrain_prop(dc_gpu_t *gpu, const fr_prop_asset_t *asset,
                               uint32_t frame, uint32_t x, uint32_t y,
                               char *err_buf, uint32_t err_cap);

#endif
