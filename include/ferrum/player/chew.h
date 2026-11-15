#ifndef FERRUM_PLAYER_CHEW_H
#define FERRUM_PLAYER_CHEW_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/gpu.h"
#include "ferrum/player/player.h"

/** Extensible item kinds; material-backed consumables only for now. */
typedef enum {
    DC_ITEM_WOOD = 1u,
    DC_ITEM_DENSE_WOOD
} dc_item_kind_t;

/** Per-player inventory: mass in Q16 per item kind, no RPG assumptions. */
typedef struct {
    uint64_t mass_fp[8u];
} fr_player_inventory_t;

void fr_player_inventory_init(fr_player_inventory_t *inventory);
uint64_t fr_player_inventory_mass(const fr_player_inventory_t *inventory,
                                  dc_item_kind_t kind);

/** Per-player chewing work state; progress carries across ticks. */
typedef struct {
    float progress;
} fr_player_chew_t;

/** One chewing tick at the world target. Effort is seconds * rate against
 * the shared physical density of the reachable wood; a completed bite
 * carves exactly one localized cell through the transactional carve
 * pipeline (unsupported wood releases through the ordinary structural
 * path) and credits the responsible inventory atomically. Invalid,
 * unreachable or non-wood targets change nothing. */
bool fr_player_chew_wood_tick(dc_gpu_t *gpu,
                         fr_player_inventory_t *inventory,
                         fr_player_chew_t *chew, const fr_player_point_t *target,
                         float effort, char *err_buf, uint32_t err_cap);

#endif
