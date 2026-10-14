#ifndef FERRUM_STRUCTURAL_AUTHORING_H
#define FERRUM_STRUCTURAL_AUTHORING_H

#include <stddef.h>
#include <stdint.h>

#include "ferrum/structural/gpu.h"

#define FR_AUTHORED_PHYSICAL_STRIDE 8u
#define FR_AUTHORED_APPEARANCE_STRIDE 64u
#define FR_AUTHORED_PIXEL_LIMIT 4194304u

enum { FR_AUTHORED_TERRAIN = 1, FR_AUTHORED_STRUCTURE = 2,
       FR_AUTHORED_RIGID = 3, FR_AUTHORED_DECORATION = 4 };

/** Immutable, preloaded selected state. Physical records are canonical
 * little-endian material-u16/role-u8/anchor-u8/object-u32 records. Appearance
 * records follow the exporter/renderer 64-byte contract. Neither pointer is
 * retained beyond placement submission. Structural member ownership is an
 * optional little-endian u32 plane; supplied graph coordinates are world/rest.
 */
typedef struct {
    const uint8_t *physical, *appearance, *member_ownership;
    size_t physical_bytes, appearance_bytes, member_ownership_bytes;
    uint32_t width, height, state_variant, state_variant_count, source_frame;
    uint8_t asset_uuid[16];
    uint64_t transaction_id;
    dc_chunk_coord_t origin;
    uint32_t origin_x, origin_y;
    int32_t pivot_x, pivot_y;
    const fr_structural_gpu_graph_t *structure;
} fr_authored_request_t;

/** Check the bounded submission envelope without scanning pixels, deriving
 * connectivity, reading GPU state, or publishing ownership. Per-pixel and
 * overlap/capacity/graph/renderer checks belong to GPU transaction preflight.
 * Coordinates are chunk plus local cell, avoiding signed64 world-cell overflow.
 */
bool fr_authored_request_valid(const fr_authored_request_t *request,
                               char *err_buf, uint32_t err_cap);

#endif
