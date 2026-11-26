#ifndef FERRUM_PLAYER_ABILITIES_H
#define FERRUM_PLAYER_ABILITIES_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/gpu.h"
#include "ferrum/player/player.h"

/** Launch a small burning coal body from a canonical world muzzle toward
 * the target. A blocked or unmapped launch footprint rejects the cast. */
bool fr_player_cast_fire_bolt(dc_gpu_t *gpu, uint64_t owner_id,
                              const fr_player_point_t *origin,
                              const fr_player_point_t *target,
                              uint32_t *out_body, char *err_buf,
                              uint32_t err_cap);
/** Launch beyond an axis-aligned caster's bounds along its aim direction. */
bool fr_player_cast_body_fire_bolt(dc_gpu_t *gpu, uint64_t owner_id,
                                   const dc_gpu_world_body_t *caster,
                                   const fr_player_point_t *target,
                                   uint32_t *out_body, char *err_buf, uint32_t err_cap);

/** Cast an area ice storm: soak the bounded disc around the target with
 * water and chill reactive cell parcels across active chunk pages.
 * Target coordinates are canonical chunk-local cells. The radius must
 * be 1 to 32 cells and the disc must fit inside the active grid. */
bool fr_player_cast_ice_storm(dc_gpu_t *gpu, const fr_player_point_t *target,
                              uint32_t radius, char *err_buf, uint32_t err_cap);

#endif
