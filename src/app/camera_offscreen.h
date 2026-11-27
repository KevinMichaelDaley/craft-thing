#ifndef DC_APP_CAMERA_OFFSCREEN_H
#define DC_APP_CAMERA_OFFSCREEN_H

#include "dungeoncraft/stream.h"
#include "level.h"

typedef enum {
    DC_CAMERA_OFFSCREEN_IDLE,
    DC_CAMERA_OFFSCREEN_SAVING,
    DC_CAMERA_OFFSCREEN_STORED,
    DC_CAMERA_OFFSCREEN_LEASED
} dc_camera_offscreen_phase_t;

typedef struct {
    struct dc_offscreen *workspace;
    dc_chunk_coord_t coord;
    dc_chunk_owner_t owner;
    dc_chunk_time_t time;
    uint64_t generation;
    unsigned water_face;
    dc_camera_offscreen_phase_t phase;
} dc_camera_offscreen_t;

/** Verify that the current physics can treat the saved tile as an isolated island. */
bool dc_camera_offscreen_isolated(const dc_level_view_t *view, const dc_stream_result_t *snapshot);
/** Identify the single open water face of an otherwise closed camera eviction. */
unsigned dc_camera_offscreen_water_face(const dc_level_view_t *view, const dc_stream_result_t *snapshot);
/** Whether the elected water owner can participate in this frame's simulation. */
bool dc_camera_offscreen_water_ready(dc_level_view_t *view);
/** Elect one eligible camera eviction after its worker request has been accepted. */
void dc_camera_offscreen_offer(dc_level_view_t *view, const dc_stream_result_t *snapshot,
    dc_chunk_owner_t owner, uint64_t write_generation);
/** Complete the election only after the matching resident save is acknowledged. */
void dc_camera_offscreen_saved(dc_level_view_t *view, const dc_stream_result_t *result);
/** Step an isolated owner or commit its confirmed state before camera return or shutdown. */
bool dc_camera_offscreen_service(dc_level_view_t *view, bool closing, char *err, uint32_t cap);
/** Release the camera-owned workspace after the level has disposed of its leases. */
void dc_camera_offscreen_discard(dc_level_view_t *view);
/** Retain the durable election for retry after releasing a failed water workspace load. */
void dc_camera_offscreen_retry(dc_camera_offscreen_t *camera);

#endif
