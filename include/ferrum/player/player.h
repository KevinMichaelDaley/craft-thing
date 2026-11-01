#ifndef FERRUM_PLAYER_PLAYER_H
#define FERRUM_PLAYER_PLAYER_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/chunk.h"

#define FR_PLAYER_KEY_CAP 64u

typedef enum {
    FR_PLAYER_ATTRIBUTE = 1, FR_PLAYER_STAT, FR_PLAYER_ABILITY
} fr_player_definition_kind_t;

/** IDs are one-based and registry-local; persist keys rather than these IDs. */
typedef struct {
    char key[FR_PLAYER_KEY_CAP];
    fr_player_definition_kind_t kind;
    double initial_value;
} fr_player_definition_t;

/** Caller-owned storage. Seal before creating players; keep alive and immutable
 * until every bound player is destroyed. Rebinding invalidates existing IDs. */
typedef struct {
    fr_player_definition_t *definitions;
    uint32_t count, capacity;
    bool sealed;
} fr_player_registry_t;

typedef enum {
    FR_PLAYER_IDLE, FR_PLAYER_WALK, FR_PLAYER_CHEW, FR_PLAYER_CAST,
    FR_PLAYER_ACTION_COUNT
} fr_player_action_t;

typedef struct { uint32_t frames, frame_ms; bool loop; } fr_player_clip_t;
/** Canonical streamed world point: local x/y in [0, DC_CHUNK_SIDE). */
typedef struct { dc_chunk_coord_t chunk; float x, y; } fr_player_point_t;
/** Press actions are supplied once by input adapters; chewing is a held intent.
 * ability=0 means no cast. Targets always belong to this player's camera. */
typedef struct {
    float move_x;
    bool jump_pressed, chew_held, has_target;
    uint32_t ability;
    fr_player_point_t target;
} fr_player_intent_t;

/** No singleton state or allocation. The session assigns unique nonzero IDs and
 * disjoint value/ability buffers per player. The registry alone may be shared.
 * Position/velocity are for the later world adapter, not a second physics solve. */
typedef struct {
    uint64_t identity;
    const fr_player_registry_t *registry;
    double *values;
    uint8_t *abilities;
    uint32_t capacity;
    fr_player_point_t position;
    float velocity_x, velocity_y;
    int32_t facing;
    bool grounded;
    fr_player_intent_t intent;
    fr_player_action_t action;
    uint64_t elapsed_ms;
} fr_player_t;

/** Bind empty definition storage; no allocation or implicit definitions. */
bool fr_player_registry_init(fr_player_registry_t *registry,
    fr_player_definition_t *storage, uint32_t capacity);
/** Register a unique namespace:name key. Ability initial_value must be zero.
 * Errors leave registry/storage/out_id unchanged. No registration after seal. */
bool fr_player_register(fr_player_registry_t *registry, const char *key,
    fr_player_definition_kind_t kind, double initial_value, uint32_t *out_id,
    char *err_buf, uint32_t err_cap);
/** Freeze definitions before player construction. */
bool fr_player_registry_seal(fr_player_registry_t *registry);
/** Return a matching typed ID, or zero for a missing/invalid key or kind. */
uint32_t fr_player_find(const fr_player_registry_t *registry, const char *key,
    fr_player_definition_kind_t kind);
/** Initialize disjoint caller-owned buffers with registry defaults and no
 * granted abilities. Invalid arguments change neither player nor buffers. */
bool fr_player_init(fr_player_t *player, uint64_t identity,
    const fr_player_registry_t *registry, double *values, uint8_t *abilities,
    uint32_t capacity);
/** Set a finite registered numeric attribute/stat; abilities are separate. */
bool fr_player_value_set(fr_player_t *player, uint32_t id, double value);
/** Read a numeric value; invalid IDs/kinds leave output unchanged. */
bool fr_player_value_get(const fr_player_t *player, uint32_t id, double *out);
/** Grant or revoke a registered ability for this player alone. */
bool fr_player_ability_set(fr_player_t *player, uint32_t id, bool granted);
/** Query an ability; unknown IDs and numeric definitions return false. */
bool fr_player_has_ability(const fr_player_t *player, uint32_t id);
/** Validate normalized movement, canonical target and granted ability before
 * publishing a complete intent. Invalid input leaves previous intent intact. */
bool fr_player_set_intent(fr_player_t *player, fr_player_intent_t intent);
/** Read placeholder timing: idle 1x100ms, walk 6x100ms looping,
 * chew 8x80ms and cast 4x100ms one-shot. Invalid action leaves output intact. */
bool fr_player_clip(fr_player_action_t action, fr_player_clip_t *out);
/** Transition resets time. Same action preserves time unless restart=true. */
bool fr_player_set_action(fr_player_t *player, fr_player_action_t action, bool restart);
/** Advance only animation time. Overflow-safe even for UINT64_MAX intervals. */
bool fr_player_advance(fr_player_t *player, uint64_t elapsed_ms);
/** Select a deterministic frame; completed one-shots hold their last frame. */
bool fr_player_frame(const fr_player_t *player, uint32_t *out_frame);
/** True once a non-looping clip's complete duration has elapsed. */
bool fr_player_action_complete(const fr_player_t *player);

#endif
