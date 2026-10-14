#ifndef FERRUM_STRUCTURAL_AUTHORING_H
#define FERRUM_STRUCTURAL_AUTHORING_H

#include <stddef.h>
#include <stdint.h>

#include "ferrum/structural/gpu.h"

#define FR_AUTHORED_PHYSICAL_STRIDE 8u
#define FR_AUTHORED_APPEARANCE_STRIDE 64u
#define FR_AUTHORED_PIXEL_LIMIT 4194304u
#define FR_AUTHORED_STATE_LIMIT 4096u

enum { FR_AUTHORED_TERRAIN = 1, FR_AUTHORED_STRUCTURE = 2,
       FR_AUTHORED_RIGID = 3, FR_AUTHORED_DECORATION = 4 };

/** Fixed GPU status words. Reservations only write demand/failure; publication
 * is permitted only after all admission participants finish with failure zero.
 */
enum { FR_AUTHORED_TX_LO = 0, FR_AUTHORED_TX_HI = 1,
       FR_AUTHORED_FAILURE = 2, FR_AUTHORED_PUBLICATION_GATE = 3,
       FR_AUTHORED_BODY_DEMAND = 4, FR_AUTHORED_GEOMETRY_WORD_DEMAND = 5,
       FR_AUTHORED_GRAPH_MEMBER_DEMAND = 6,
       FR_AUTHORED_APPEARANCE_DEMAND = 7, FR_AUTHORED_PROVENANCE_DEMAND = 8,
       FR_AUTHORED_EMISSION_DEMAND = 9, FR_AUTHORED_APPEARANCE_INSTANCE = 10,
       FR_AUTHORED_INSTANCE_GENERATION_LO = 11,
       FR_AUTHORED_INSTANCE_GENERATION_HI = 12,
       FR_AUTHORED_INPUT_DIAGNOSTIC = 13, FR_AUTHORED_COMPLETION = 14,
       FR_AUTHORED_STATUS_WORDS = 16 };
#define FR_AUTHORED_INVALID_OUTPUT UINT32_MAX
enum { FR_AUTHORED_FAIL_INPUT = 1, FR_AUTHORED_FAIL_UNKNOWN = 2,
       FR_AUTHORED_FAIL_OVERLAP = 4, FR_AUTHORED_FAIL_CAPACITY = 8,
       FR_AUTHORED_FAIL_GENERATION = 16, FR_AUTHORED_FAIL_REPLAY = 32,
       FR_AUTHORED_FAIL_APPEARANCE = 64 };

/** Renderer-owned immutable provenance; runtime body IDs never replace
 * appearance identity. Source coordinates are row-major selected-state texels.
 */
typedef struct {
    uint64_t appearance_identity;
    uint32_t source_pixel, source_frame;
} fr_authored_provenance_t;

/** One aligned source-to-output GPU record. Zero role means absent source.
 * geometry_word is the absolute reserved sparse shape-header word. rest_x/y
 * resolve that shape's tile directory; tile_cell is the target16x16 cell0..255.
 * Terrain/decoration body_slot and geometry_word are FR_AUTHORED_INVALID_OUTPUT.
 * Terrain uses canonical chunk/local coordinates. Decoration keeps instance/rest
 * ownership without acquiring a body slot or adding material pixels.
 */
typedef struct {
    uint32_t role, object, component, body_slot;
    uint32_t geometry_word, tile_cell, source_pixel, source_frame;
    dc_chunk_coord_t chunk;
    uint32_t local_x, local_y, rest_x, rest_y;
} fr_authored_destination_t;

/** Backend buffer token and byte extent; callbacks record GPU work rather than
 * dereferencing these buffers or reading their status to the CPU.
 */
typedef struct { uint64_t token, offset_bytes, bytes; } fr_authored_buffer_view_t;

/** GPU-visible admission plan passed at a completed staging boundary. Vulkan
 * adapters interpret buffer tokens as non-dispatchable buffer handles.
 */
typedef struct {
    fr_authored_buffer_view_t status, physical, appearance, destinations;
    uint32_t width, height, source_frame, state_variant;
    uint8_t asset_uuid[16];
    uint64_t transaction_id;
} fr_authored_gpu_plan_t;

/** Optional renderer admission participant. Each callback records commands into
 * the transaction's current GPU command buffer. reserve must set shared failure
 * bits on GPU capacity/generation failure; transfer and publish must observe the
 * same gate. False means command-recording failure and aborts the submission.
 * Physics owns ordering/barriers; participant storage is never allocated by
 * physics. No callback may advance time or perform runtime GPU readback.
 */
typedef struct {
    void *context;
    bool (*reserve)(dc_gpu_t *, const fr_authored_gpu_plan_t *, void *,
                    char *, uint32_t);
    bool (*transfer)(dc_gpu_t *, const fr_authored_gpu_plan_t *, void *,
                     char *, uint32_t);
    bool (*publish)(dc_gpu_t *, const fr_authored_gpu_plan_t *, void *,
                    char *, uint32_t);
} fr_authored_admission_t;

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
