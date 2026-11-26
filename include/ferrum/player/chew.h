#ifndef FERRUM_PLAYER_CHEW_H
#define FERRUM_PLAYER_CHEW_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/gpu.h"
#include "ferrum/player/player.h"

/** Extensible item kinds; material-backed consumables only for now. */
typedef enum {
    DC_ITEM_WOOD = 1u,
    DC_ITEM_DENSE_WOOD,
    DC_ITEM_STONE
} dc_item_kind_t;

/** Per-player inventory: mass in Q16 per item kind, no RPG assumptions. */
typedef struct {
    uint64_t mass_fp[8u];
} fr_player_inventory_t;

/** Reset all material balances. */
void fr_player_inventory_init(fr_player_inventory_t *inventory);
/** Return the dry mass held for one inventory kind. */
uint64_t fr_player_inventory_mass(const fr_player_inventory_t *inventory,
                                  dc_item_kind_t kind);
/** Map a harvestable terrain material to its inventory kind, or return zero. */
dc_item_kind_t fr_player_inventory_kind(uint16_t material);

/** One chewable localized wood pocket: identity, eligibility and the
 * occupied amount priced at the shared physical density. */
typedef struct {
    uint64_t parcel_id;
    bool reachable;
    uint16_t material;
    uint32_t dry_mass_fp;
    bool eligible;
    uint32_t generation;
} fr_chew_target_t;

/** Deterministic chewing work bank: effort-ms accrued toward one bite,
 * pinned to a single target identity so retries and cadence changes
 * neither duplicate nor lose work. */
typedef struct {
    uint64_t work;
    uint64_t parcel_id;
    uint32_t generation;
    uint32_t dry_mass_fp;
    uint16_t material;
    bool armed;
} fr_chew_work_t;

/** Advance chewing work by effort*delta_ms against the target. A target
 * that is absent, ineligible or changed resets the bank transactionally;
 * reaching the pocket's dry mass banks exactly one bite and reports it
 * once through bite_ready without ever banking a second bite. */
bool fr_chew_work_advance(fr_chew_work_t *work,const fr_chew_target_t *target,
    uint32_t effort,uint64_t delta_ms,bool chewing,bool *bite_ready);

/** Acknowledge the banked bite for this exact target identity: true once,
 * clearing the bank; false for stale identities or without a banked bite. */
bool fr_chew_work_committed(fr_chew_work_t *work,const fr_chew_target_t *target);

/** Per-player work tied to one world cell and its resident owner. */
typedef struct {
    fr_chew_work_t bank;
    dc_chunk_coord_t chunk;
    uint32_t local, slot;
} fr_player_chew_t;

/** Resolve a reachable terrain cell at or adjacent to the aim and advance
 * its harvest work by elapsed_ms while held. A NULL aim clears the bank.
 * Release pauses progress while the same material and owner remain valid.
 * A committed terrain cut credits its dry mass to this inventory once. */
bool fr_player_harvest_tick(dc_gpu_t *gpu, fr_player_inventory_t *inventory,
    fr_player_chew_t *chew, const fr_player_point_t *origin,
    const fr_player_point_t *aim, bool held, uint64_t elapsed_ms,
    bool *eligible, char *err_buf, uint32_t err_cap);

#endif
