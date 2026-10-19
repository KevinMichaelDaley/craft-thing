#ifndef FERRUM_RENDERER_APPEARANCE_H
#define FERRUM_RENDERER_APPEARANCE_H

#include <stdbool.h>
#include <stdint.h>

enum { FR_APPEARANCE_TERRAIN = 0, FR_APPEARANCE_BODY = 1 };

/** Immutable texel owner; coordinates are world cells or body rest cells. */
typedef struct {
  uint64_t identity, generation;
  int64_t x, y;
  uint32_t pixel_index, material, domain, visual_scale;
} fr_appearance_owner_t;

/** Renderer sidecar in linear RGB, independent of physical material and
 * coverage. */
typedef struct {
  float color[3], roughness;
  float normal[3], relief;
} fr_appearance_texel_t;

enum {
  FR_APPEARANCE_NORMAL = 1,
  FR_APPEARANCE_ROUGHNESS = 2,
  FR_APPEARANCE_RELIEF = 4
};

/** Native GPU view of the compiled prop's 64-byte appearance record. */
typedef struct {
  fr_appearance_texel_t surface;
  float emission[3], reserved_emission;
  float alpha;
  uint32_t reserved[2], surface_mask;
} fr_appearance_source_t;

/** Resolve one imported texel against material defaults, preserving linear HDR
 * emission and visual alpha. Invalid input leaves output unchanged. This is a
 * reference conversion; runtime admission converts source texels in parallel.
 */
bool fr_appearance_import(const fr_appearance_source_t *source,
                          const fr_appearance_texel_t *defaults,
                          fr_appearance_source_t *output);

/** Caller-owned fixed storage; generation changes clear validity, not physics.
 */
typedef struct {
  fr_appearance_owner_t owner;
  fr_appearance_texel_t *texels;
  uint32_t *owner_indices;
  uint8_t *valid;
  uint32_t capacity;
} fr_appearance_sidecar_t;

/** Bind fixed sidecar storage and invalidate previous slot contents. */
bool fr_appearance_bind(fr_appearance_sidecar_t *sidecar,
                        fr_appearance_owner_t owner,
                        fr_appearance_texel_t *texels, uint32_t *owner_indices,
                        uint8_t *valid, uint32_t capacity);

/** Write explicit color/surface override without changing physical ownership.
 */
bool fr_appearance_write(fr_appearance_sidecar_t *sidecar, uint32_t texel,
                         uint32_t owner_index,
                         fr_appearance_texel_t appearance);

/** Fetch an override only when the owning slot generation still matches. */
bool fr_appearance_read(const fr_appearance_sidecar_t *sidecar, uint32_t texel,
                        uint64_t identity, uint64_t generation,
                        uint32_t *owner_index,
                        fr_appearance_texel_t *appearance);

/** Resolve visual coordinates to a physical owner index; holes remain holes. */
bool fr_appearance_resolve(uint32_t width, uint32_t height, uint32_t scale,
                           uint32_t x, uint32_t y, const uint32_t *materials,
                           uint32_t *pixel_index, uint32_t *material);

/** Reject a recycled slot even when the pixel index and material match. */
bool fr_appearance_matches(const fr_appearance_owner_t *owner,
                           uint64_t identity, uint64_t generation);

/** Convert terrain or body material into the terrain namespace explicitly. */
bool fr_appearance_material(uint32_t domain, uint32_t material,
                            uint32_t *terrain_material);

/** Deterministic old-world defaults; never reads or modifies simulation cells.
 */
fr_appearance_texel_t fr_appearance_default(fr_appearance_owner_t owner);

/** Inverse rigid transform into rest-space coordinates, preserving appearance.
 */
void fr_appearance_rest_point(float x, float y, float origin_x, float origin_y,
                              float angle, float *rest_x, float *rest_y);

#endif
