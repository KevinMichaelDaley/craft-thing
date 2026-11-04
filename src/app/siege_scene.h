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

/** Projectile load staged in the pouch. Coal is the locked burning-coal
 * shot; oil and greek stage an inert stone core with a pre-armed ordinary
 * oil or oil/jelly/bound-oxidizer coating for the Greek-fire module. */
typedef enum {
    DC_SIEGE_LOAD_COAL,
    DC_SIEGE_LOAD_OIL,
    DC_SIEGE_LOAD_GREEK
} dc_siege_load_t;

/** Build the locked siege: fixed stone floor, 8x80 dry or saturated wood wall,
 * supported launcher frame, cocked arm, revolute pivot/latch/pouch and one
 * burning coal ball. The wet control saturates the wall at 0.95. */
bool dc_siege_scene_build(dc_gpu_t *gpu, bool wet_wall, bool hot_coal,
                          dc_siege_scene_t *out, char *err_buf, uint32_t err_cap);

/** Build the siege with an explicit projectile load (coal, ordinary oil or
 * Greek fire). Oil and Greek loads carry matched pre-armed coating fuel on
 * an inert stone core so coating chemistry is the only fuel difference. */
bool dc_siege_scene_build_load(dc_gpu_t *gpu, bool wet_wall, dc_siege_load_t load,
                               dc_siege_scene_t *out, char *err_buf,
                               uint32_t err_cap);
/** Build the siege with a stone-rimmed water pool east of the wall so a
 * launched load splashes down submerged; used by the dousing controls. */
bool dc_siege_scene_build_douse_pool(dc_gpu_t *gpu, dc_siege_load_t load,
                                     dc_siege_scene_t *out, char *err_buf,
                                     uint32_t err_cap);

/** Build the launcher-risk fixture: the load waits in the pouch with the
 * given seed folded into parcel identity, so per-seed ignition thresholds
 * form a locked deterministic ensemble. The fireproof variant stages the
 * carrier arm in stone. */
bool dc_siege_scene_build_risk(dc_gpu_t *gpu, dc_siege_load_t load,
                               uint32_t seed, bool fireproof,
                               dc_siege_scene_t *out, char *err_buf,
                               uint32_t err_cap);

/** Light the staged oil or greek coating on the waiting projectile after
 * fuse_seconds of burn-down (a struck fuse flares late, letting a launched
 * load clear the carrier before it flames; zero lights it immediately). */
bool dc_siege_scene_ignite_load(dc_gpu_t *gpu, dc_siege_scene_t *scene,
                                float fuse_seconds, char *err_buf,
                                uint32_t err_cap);
/** Build the siege with a stone-rimmed water pool east of the wall so a
 * launched load splashes down submerged; used by the dousing controls. */
bool dc_siege_scene_build_douse_pool(dc_gpu_t *gpu, dc_siege_load_t load,
                                     dc_siege_scene_t *out, char *err_buf,
                                     uint32_t err_cap);

/** Build the launcher-risk fixture: the load waits in the pouch with the
 * given seed folded into parcel identity, so per-seed ignition thresholds
 * form a locked deterministic ensemble. The fireproof variant stages the
 * carrier arm in stone. */
bool dc_siege_scene_build_risk(dc_gpu_t *gpu, dc_siege_load_t load,
                               uint32_t seed, bool fireproof,
                               dc_siege_scene_t *out, char *err_buf,
                               uint32_t err_cap);

/** Light the staged oil or greek coating on the waiting projectile after
 * fuse_seconds of burn-down (a struck fuse flares late, letting a launched
 * load clear the carrier before it flames; zero lights it immediately). */
bool dc_siege_scene_ignite_load(dc_gpu_t *gpu, dc_siege_scene_t *scene,
                                float fuse_seconds, char *err_buf,
                                uint32_t err_cap);

/** Release the cocked arm without changing any body pose or velocity. */
bool dc_siege_scene_release(dc_gpu_t *gpu, dc_siege_scene_t *scene,
                            char *err_buf, uint32_t err_cap);

/** Open the coal pouch after arm acceleration, preserving its motion. */
bool dc_siege_scene_release_projectile(dc_gpu_t *gpu, dc_siege_scene_t *scene,
                                       char *err_buf, uint32_t err_cap);

#endif
