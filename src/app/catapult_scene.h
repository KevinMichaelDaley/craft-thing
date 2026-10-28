#ifndef DC_CATAPULT_SCENE_H
#define DC_CATAPULT_SCENE_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/gpu.h"

typedef struct {
    uint32_t frame_body;
    uint32_t wall_body;
    uint32_t arm_body;
    uint32_t coal_body;
    uint32_t pivot_joint;
    uint32_t latch_joint;
    bool released;
} dc_catapult_scene_t;

/** Build a supported launcher and a finite coal load in a 128-cell test world. */
bool dc_catapult_scene_build(dc_gpu_t *gpu, bool hot,
                             dc_catapult_scene_t *scene,
                             char *err_buf, uint32_t err_cap);

/** Open the physical coal latch without changing the projectile pose or velocity. */
bool dc_catapult_scene_release(dc_gpu_t *gpu, dc_catapult_scene_t *scene,
                               char *err_buf, uint32_t err_cap);

#endif
