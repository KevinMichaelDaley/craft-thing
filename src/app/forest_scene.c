#include <stdio.h>
#include <string.h>

#include "forest_scene.h"

#include <math.h>

bool dc_forest_scene_build(dc_gpu_t *gpu, bool dry_bridge, bool broken_ring,
                           dc_forest_scene_t *out, char *err_buf, uint32_t err_cap) {
    (void)gpu;(void)dry_bridge;(void)broken_ring;(void)out;
    if (err_buf && err_cap)
        snprintf(err_buf, err_cap, "Forest bonfire scene is not implemented");
    return false;
}
