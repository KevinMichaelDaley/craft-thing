#ifndef DC_CATAPULT_SCENE_H
#define DC_CATAPULT_SCENE_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/gpu.h"

typedef struct {
    uint32_t frame_body;
    uint32_t base_body;
    uint32_t wall_body;
    uint32_t wall_upper_body;
    uint32_t arm_body;
    uint32_t coal_body;
    uint32_t pivot_joint;
    uint32_t latch_joint;
    uint32_t pouch_joint;
    bool released;
    bool projectile_released;
} dc_catapult_scene_t;

/** Build a supported launcher, tall stone wall, wooden interior battlement,
 * and finite coal load in a 256x128 world with a stone floor at y=96. */
bool dc_catapult_scene_build(dc_gpu_t *gpu, bool hot,
                             dc_catapult_scene_t *scene,
                             char *err_buf, uint32_t err_cap);

/** Build the scene in a larger view with the supplied chunk row stride. */
bool dc_catapult_scene_build_with_chunk_stride(dc_gpu_t *gpu, bool hot,
                                                uint32_t chunk_stride,
                                                dc_catapult_scene_t *scene,
                                                char *err_buf,
                                                uint32_t err_cap);

/** Release the cocked arm without changing any body pose or velocity. */
bool dc_catapult_scene_release(dc_gpu_t *gpu, dc_catapult_scene_t *scene,
                               char *err_buf, uint32_t err_cap);

/** Open the coal pouch after arm acceleration, preserving its current motion. */
bool dc_catapult_scene_release_projectile(dc_gpu_t *gpu,
                                          dc_catapult_scene_t *scene,
                                          char *err_buf, uint32_t err_cap);

#endif
