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

/** Verify the two mapped endpoints of the positive world-axis face. */
bool dc_gpu_liquid_seam_mapped(dc_liquid_seam_endpoint_t first,
    dc_liquid_seam_endpoint_t second, dc_liquid_seam_axis_t axis);

/** Projected normal speed in cells per reference tick, native pressure at
 * each cell, and the open fraction of the whole face in Q16. */
typedef struct {
    float normal_velocity;
    float first_pressure, second_pressure;
    uint32_t aperture_fp;
} dc_liquid_seam_sample_t;

/** Sample the native liquid projection on the sealed domain of the two tiles.
 * Take the snapshot at a common owner interval with current cells, occupancy and
 * face speeds. The caller holds both mapped owners for the duration of the call.
 * Results are indexed by the transverse coordinate, with positive velocity toward
 * the second tile. Pass a previous normal to carry the shared speed between samples.
 * Only the scratch projection advances and samples are written on completion.
 */
bool dc_gpu_project_liquid_seam(dc_liquid_seam_endpoint_t first,
    dc_liquid_seam_endpoint_t second, dc_liquid_seam_axis_t axis, float seconds,
    const float previous_normal[DC_CHUNK_SIDE],
    dc_liquid_seam_sample_t samples[DC_CHUNK_SIDE], char *err, uint32_t cap);

/** Water stock and gross crossing volumes for one original owner cell, in Q16. */
typedef struct {
    uint32_t initial_fp, final_fp;
    uint32_t crossing_out_fp, crossing_in_fp;
} dc_water_seam_cell_t;

/** Expected and final carried water fields in one owner cell. */
typedef struct {
    float initial_color[4], final_color[4];
    float initial_excess_heat, final_excess_heat;
} dc_water_seam_payload_t;

/** The scratch water sweep's write set, in original owner chunk order. */
typedef struct {
    dc_liquid_seam_sample_t projection[DC_CHUNK_SIDE];
    int32_t crossing_fp[DC_CHUNK_SIDE];
    float pressure_work[DC_CHUNK_SIDE];
    dc_water_seam_cell_t cells[2][DC_CHUNK_CELLS];
    dc_water_seam_payload_t payload[2][DC_CHUNK_CELLS];
    bool thermal_enabled;
} dc_water_seam_plan_t;

/** Plan water transport at a common owner interval on the projected two-tile domain.
 * Signed flow is positive from first to second. Pressure work is the native
 * pressure difference times net volume in cells. Input owners remain live and
 * read-only. The plan is published on success. Expected and final heat
 * contain zero in worlds with thermal simulation disabled.
 */
bool dc_gpu_plan_water_seam(dc_liquid_seam_endpoint_t first,
    dc_liquid_seam_endpoint_t second, dc_liquid_seam_axis_t axis, float seconds,
    const float previous_normal[DC_CHUNK_SIDE], dc_water_seam_plan_t *plan,
    char *err, uint32_t cap);

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
