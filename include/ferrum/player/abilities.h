#ifndef FERRUM_PLAYER_ABILITIES_H
#define FERRUM_PLAYER_ABILITIES_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/gpu.h"
#include "ferrum/player/player.h"

/** Cast a fire bolt: author a small burning coal charge at the caster,
 * admit it as a rigid body and launch it toward the target at the
 * engine's capped projectile speed. Ignition on impact flows through
 * the ordinary combustion brand path. */
bool fr_player_cast_fire_bolt(dc_gpu_t *gpu, uint64_t owner_id,
                              const fr_player_point_t *origin,
                              const fr_player_point_t *target,
                              uint32_t *out_body, char *err_buf,
                              uint32_t err_cap);

/** Cast an area ice storm: soak the bounded disc around the target with
 * water and chill every reactive parcel inside the radius. Radius must
 * be at least one cell. */
bool fr_player_cast_ice_storm(dc_gpu_t *gpu, const fr_player_point_t *target,
                              uint32_t radius, char *err_buf, uint32_t err_cap);

#endif
