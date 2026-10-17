#include <stdio.h>

#include "ferrum/assets/prop_gpu.h"

bool fr_gpu_place_terrain_prop(dc_gpu_t *gpu, const fr_prop_asset_t *asset,
                               uint32_t frame, uint32_t x, uint32_t y,
                               char *err_buf, uint32_t err_cap) {
    (void)gpu; (void)asset; (void)frame; (void)x; (void)y;
    if (err_buf && err_cap) snprintf(err_buf, err_cap, "Terrain prop placement not implemented");
    return false;
}
