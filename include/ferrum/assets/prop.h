#ifndef FERRUM_ASSETS_PROP_H
#define FERRUM_ASSETS_PROP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *bytes;
    size_t size;
    uint32_t width, height, frame_count, flags, data_offset;
} fr_prop_asset_t;

typedef struct {
    uint16_t material;
    uint8_t role, anchor;
    uint32_t object;
} fr_prop_pixel_t;

typedef struct {
    float color[3], roughness;
    float normal[3], relief;
    float emission[3], alpha;
    uint32_t surface_mask;
} fr_prop_appearance_t;

/** Validate a preloaded compiled asset without allocations; leave output unchanged on failure. */
bool fr_prop_asset_open(fr_prop_asset_t *asset, const void *bytes, size_t size,
                        char *err_buf, uint32_t err_cap);
/** Read one authoritative physical pixel from a validated asset frame. */
bool fr_prop_pixel(const fr_prop_asset_t *asset, uint32_t frame, uint32_t pixel,
                   fr_prop_pixel_t *out);
/** Read independent linear appearance and emission from a validated asset frame. */
bool fr_prop_appearance(const fr_prop_asset_t *asset, uint32_t frame, uint32_t pixel,
                        fr_prop_appearance_t *out);
/** Convert canonical prop material to the explicit terrain registry; unsupported returns zero. */
uint32_t fr_prop_terrain_material(uint32_t material);
/** Convert canonical prop material to body registry; unsupported returns zero. */
uint32_t fr_prop_body_material(uint32_t material);
/** Select full-animation forward-loop frame by elapsed milliseconds (no physics changes). */
uint32_t fr_prop_animation_frame(const fr_prop_asset_t *asset, uint64_t elapsed_ms);

#endif
