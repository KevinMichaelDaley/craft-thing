#ifndef DC_APP_PLAYER_INVENTORY_SAVE_H
#define DC_APP_PLAYER_INVENTORY_SAVE_H

#include "dungeoncraft/world_save.h"
#include "ferrum/player/chew.h"
#include "ferrum/player/input.h"

#define DC_PLAYER_TRANSACTION_BASE UINT64_C(0x4245415600000000)

/** Read and validate all local player balances. Legacy worlds start empty. */
bool dc_player_inventory_load(const dc_world_save_t *world, fr_player_inventory_t *balances,
                              char *err, uint32_t cap);

/** Store all local player balances, including seats not currently joined. */
bool dc_player_inventory_stage(const dc_world_save_t *world, const fr_player_inventory_t *balances,
                               char *err, uint32_t cap);

#endif
