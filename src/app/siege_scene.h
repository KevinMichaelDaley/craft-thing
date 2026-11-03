#ifndef DC_APP_SIEGE_SCENE_H
#define DC_APP_SIEGE_SCENE_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/gpu.h"

/* Locked burning-coal siege fixture geometry (assets/fixtures/fire_smoke_v1.json). */
enum {
    DC_SIEGE_WALL_X = 154, DC_SIEGE_WALL_Y = 150,
    DC_SIEGE_WALL_W = 8, DC_SIEGE_WALL_H = 80,
    DC_SIEGE_PIVOT_X = 104, DC_SIEGE_PIVOT_Y = 188,
    /* The authored arm keeps one air row above the throwing bar so the
     * authored extraction separates the rigid arm from the fixed frame. */
    DC_SIEGE_ARM_X = 102, DC_SIEGE_ARM_Y = 151,
    DC_SIEGE_ARM_W = 4, DC_SIEGE_ARM_H = 36,
    DC_SIEGE_COAL_RADIUS = 4,
    DC_SIEGE_VIEW_W = 480, DC_SIEGE_VIEW_H = 270,
    DC_SIEGE_VIEWPORT_COVERAGE_MAX_PCT = 25u
};

typedef struct {
    uint64_t wall_fuel_fp, wall_water_fp;
    uint32_t coal_body, arm_body;
    uint32_t pivot_joint, latch_joint, pouch_joint;
    uint32_t wall_cells, coal_cells;
} dc_siege_scene_t;

/** Build the locked siege: fixed stone floor, 8x80 dry or saturated wood wall,
 * supported launcher frame, cocked arm, revolute pivot/latch/pouch and one
 * burning coal ball. The wet control saturates the wall at 0.95. */
bool dc_siege_scene_build(dc_gpu_t *gpu, bool wet_wall, bool hot_coal,
                          dc_siege_scene_t *out, char *err_buf, uint32_t err_cap);

/** Release the cocked arm without changing any body pose or velocity. */
bool dc_siege_scene_release(dc_gpu_t *gpu, dc_siege_scene_t *scene,
                            char *err_buf, uint32_t err_cap);

/** Open the coal pouch after arm acceleration, preserving its motion. */
bool dc_siege_scene_release_projectile(dc_gpu_t *gpu, dc_siege_scene_t *scene,
                                       char *err_buf, uint32_t err_cap);

#endif
