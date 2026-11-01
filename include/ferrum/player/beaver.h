#ifndef FERRUM_PLAYER_BEAVER_H
#define FERRUM_PLAYER_BEAVER_H

#include <stddef.h>
#include <stdint.h>

#include "ferrum/assets/prop.h"
#include "ferrum/fire/reactive_state.h"
#include "ferrum/player/player.h"
#include "ferrum/renderer/appearance.h"

#define FR_BEAVER_WIDTH 40u
#define FR_BEAVER_HEIGHT 40u
#define FR_BEAVER_PIXELS (FR_BEAVER_WIDTH * FR_BEAVER_HEIGHT)

typedef enum {
    FR_BEAVER_EMPTY, FR_BEAVER_FUR, FR_BEAVER_ROBE, FR_BEAVER_HAT,
    FR_BEAVER_TAIL, FR_BEAVER_TEETH, FR_BEAVER_REGION_COUNT
} fr_beaver_region_t;

/** Immutable source metadata, independent of a frame's position or facing.
 * Placeholder fuel regions explicitly reuse the supported wood profile;
 * teeth reuse stone. Region identity is retained for future physical tuning. */
typedef struct {
    fr_beaver_region_t region;
    uint32_t prop_material;
} fr_beaver_source_t;

/** Cold-built standard CPRP clip plus caller-owned canonical source mapping.
 * Mapping entries are source rest pixel+1; zero is transparent. These assets
 * declare explicit state variants. Runtime must bind canonical parcels and
 * must never place/reseed a new world prop for each animation frame. */
typedef struct {
    fr_prop_asset_t prop;
    const uint32_t *source_pixels;
    uint32_t source_count;
    fr_player_action_t action;
} fr_beaver_asset_t;

/** Return required compiled bytes and mapping entries for an action clip.
 * Supports idle as well as exact six/eight/four walk/chew/cast frame counts.
 * Invalid arguments leave outputs unchanged. */
bool fr_beaver_asset_size(fr_player_action_t action, size_t *bytes, uint32_t *entries);
/** Deterministically author a standard compiled clip in caller-owned buffers.
 * Cold initialization only; no heap allocation. Failures due to invalid input
 * or capacity leave buffers and output unchanged. Buffers must be disjoint and
 * remain alive/immutable while the asset is used. */
bool fr_beaver_asset_build(fr_player_action_t action, uint8_t *bytes, size_t capacity,
    uint32_t *mapping, uint32_t mapping_capacity, fr_beaver_asset_t *out,
    char *err_buf, uint32_t err_cap);
/** Sample a preloaded frame with optional horizontal mirror. Transparent pixels
 * return material=0/source=0; out-of-bounds/invalid inputs leave outputs intact. */
bool fr_beaver_asset_pixel(const fr_beaver_asset_t *asset, uint32_t frame,
    uint32_t x, uint32_t y, bool flip, fr_prop_pixel_t *out, uint32_t *source);
/** Read immutable region/material metadata for a nonzero canonical source ID. */
bool fr_beaver_source(uint32_t source, fr_beaver_source_t *out);
/** Bind one canonical source pixel to existing BODY rest-coordinate contracts.
 * Caller supplies the actual distinct runtime body and generation, and a stable
 * appearance identity (normally player identity). Does not seed or copy state.
 * Animation, world movement and facing never enter these owner keys. */
bool fr_beaver_pixel_owners(uint32_t source, uint64_t appearance_identity,
    uint32_t body_id, uint32_t generation, fr_appearance_owner_t *appearance,
    fr_reactive_owner_t *reactive);
/** Bind a canonical source through an extraction-trimmed body rest origin.
 * rest_origin_x/y are canonical canvas coordinates of that body's rest (0,0).
 * Appearance keeps canonical coordinates; reactive keys use actual body rest.
 * Reject sources outside the supplied body extent without changing outputs.
 * The caller obtains origin/extent from admission, never a frame-time readback. */
bool fr_beaver_pixel_owners_trimmed(uint32_t source,uint64_t appearance_identity,
    uint32_t body_id,uint32_t generation,uint32_t rest_origin_x,uint32_t rest_origin_y,
    uint32_t width,uint32_t height,fr_appearance_owner_t *appearance,
    fr_reactive_owner_t *reactive);

#endif
