#ifndef FERRUM_PLAYER_PLACEMENT_H
#define FERRUM_PLAYER_PLACEMENT_H

#include "ferrum/player/chew.h"

/** Place one terrain cell or a typed rigid block at a canonical world aim.
 * Unavailable, occupied or unaffordable destinations leave placed false.
 * The inventory pays only for material published to the world. */
bool fr_player_place_material(dc_gpu_t *gpu, fr_player_inventory_t *inventory,
                              const fr_player_point_t *aim, uint16_t material,
                              bool rigid, bool *placed, char *err, uint32_t cap);

#endif
