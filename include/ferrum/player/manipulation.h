#ifndef FERRUM_PLAYER_MANIPULATION_H
#define FERRUM_PLAYER_MANIPULATION_H

#include <stdbool.h>

#include "dungeoncraft/gpu.h"
#include "dungeoncraft/chunk.h"
#include "ferrum/player/player.h"

/** Per-player carry runtime: which body this player is holding. */
typedef struct { uint32_t body_id; } fr_player_carry_t;

/** Per-player manipulation attributes: modifiable by stats and magic
 * effects through whatever progression system owns them. */
typedef struct {
    double carry_weight; /**< Mass units this player can hold. */
    double throw_force;  /**< Launch speed in cells per tick. */
    double reach;        /**< Grab distance from the player anchor. */
} fr_player_manipulation_t;

/** Select the nearest dynamic body under the reticle within reach whose
 * mass fits the carry weight, and hold it. Fails (state unchanged) when
 * nothing qualifies or something is already carried. */
bool fr_player_manipulation_grab(dc_gpu_t *gpu,fr_player_carry_t *carry,
    const fr_player_manipulation_t *m,float player_x,float player_y,
    const fr_player_point_t *reticle,char *err_buf,uint32_t err_cap);

/** Carry tick: drive the carried body to the hold point over the player. */
bool fr_player_manipulation_tick(dc_gpu_t *gpu,fr_player_carry_t *carry,
    float player_x,float player_y,char *err_buf,uint32_t err_cap);

/** Release the carried body in place, preserving its current motion. */
bool fr_player_manipulation_drop(dc_gpu_t *gpu,fr_player_carry_t *carry,
    char *err_buf,uint32_t err_cap);

/** Release and launch the carried body toward the reticle at the throw
 * force speed (capped by the engine's four-cells-per-tick bound). */
bool fr_player_manipulation_throw(dc_gpu_t *gpu,fr_player_carry_t *carry,
    const fr_player_manipulation_t *m,float player_x,float player_y,
    const fr_player_point_t *reticle,char *err_buf,uint32_t err_cap);

/** True while the given body is the one being carried. */
bool fr_player_manipulation_carrying(const fr_player_carry_t *carry,uint32_t body_id);

/** Drop any carried body silently (input focus loss, death, teardown). */
void fr_player_manipulation_reset(fr_player_carry_t *carry);

#endif
