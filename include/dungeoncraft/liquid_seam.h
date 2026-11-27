#ifndef DUNGEONCRAFT_LIQUID_SEAM_H
#define DUNGEONCRAFT_LIQUID_SEAM_H

#include "dungeoncraft/gpu.h"

typedef enum { DC_LIQUID_SEAM_X, DC_LIQUID_SEAM_Y } dc_liquid_seam_axis_t;

typedef struct {
    dc_gpu_t *gpu;
    dc_chunk_coord_t coord;
    uint32_t tile_x, tile_y, slot;
} dc_liquid_seam_endpoint_t;

/** Signed Q16 volumes indexed by the transverse coordinate of the face. */
typedef struct {
    int32_t water, oil, slime;
} dc_liquid_seam_flux_t;

typedef enum {
    DC_LIQUID_SEAM_REJECTED,
    DC_LIQUID_SEAM_COMMITTED,
    DC_LIQUID_SEAM_UNCONFIRMED
} dc_liquid_seam_result_t;

/** Commit a complete face plan on the shared Vulkan queue. The second endpoint
 * is one chunk in the positive axis direction from the first. Positive flux
 * goes from first to second. Rejected plans leave both endpoints unchanged.
 * The caller owns the generations and epoch of both endpoints for the call.
 * After an unconfirmed submission the caller must recover both endpoints.
 */
dc_liquid_seam_result_t dc_gpu_commit_liquid_seam(dc_liquid_seam_endpoint_t first,
    dc_liquid_seam_endpoint_t second, dc_liquid_seam_axis_t axis,
    const dc_liquid_seam_flux_t flux[DC_CHUNK_SIDE], char *err, uint32_t cap);

/** Reconcile an unconfirmed dispatch through its retained GPU receipt. */
dc_liquid_seam_result_t dc_gpu_resolve_liquid_seam(dc_gpu_t *first, dc_gpu_t *second,
    char *err, uint32_t cap);

#endif
