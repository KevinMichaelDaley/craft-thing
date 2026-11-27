#ifndef DC_APP_CAMERA_OFFSCREEN_H
#define DC_APP_CAMERA_OFFSCREEN_H

#include "dungeoncraft/stream.h"
#include "level.h"

enum { DC_CAMERA_WORKSPACE_LIMIT = 16u };

typedef enum {
    DC_CAMERA_OFFSCREEN_IDLE,
    DC_CAMERA_OFFSCREEN_SAVING,
    DC_CAMERA_OFFSCREEN_STORED,
    DC_CAMERA_OFFSCREEN_LEASED
} dc_camera_offscreen_phase_t;

typedef struct {
    struct dc_offscreen *workspace;
    struct dc_camera_bond *bond;
    dc_chunk_coord_t coord;
    dc_chunk_owner_t owner;
    dc_chunk_time_t time;
    uint64_t generation;
    unsigned water_face;
    bool rigid_group, parking, restored_record;
    dc_camera_offscreen_phase_t phase;
} dc_camera_offscreen_t;

/** Inspect sealed owners that require a coordinated catalog write on close. */
bool dc_camera_catalog_active(const dc_level_view_t *view);
/** Stage confirmed room receipts after the chunk writes finish. */
bool dc_camera_catalog_stage(dc_level_view_t *view, char *err, uint32_t cap);
/** Restore sealed room elections and protect the canonical catalog during play. */
bool dc_camera_catalog_load(dc_level_view_t *view, char *err, uint32_t cap);
/** Find a tracked camera room by coordinate. */
dc_camera_offscreen_t *dc_camera_find_room(dc_level_view_t *view, dc_chunk_coord_t coord);
/** Verify that the current physics can treat the saved tile as an isolated island. */
bool dc_camera_offscreen_isolated(const dc_level_view_t *view, const dc_stream_result_t *snapshot);
/** Identify the single open water face of an otherwise closed camera eviction. */
unsigned dc_camera_offscreen_water_face(const dc_level_view_t *view, const dc_stream_result_t *snapshot);
/** Whether the elected offscreen owner can participate in this frame's simulation. */
bool dc_camera_offscreen_ready(dc_level_view_t *view);
/** Check the current mapped owners before changing the camera during a water cycle. */
bool dc_camera_offscreen_hold_remap(dc_level_view_t *view, bool *hold, char *err, uint32_t cap);
/** Reserve a durable-owner tracking entry before queuing a resident save. */
bool dc_camera_offscreen_reserve(dc_level_view_t *view);
/** Whether a camera room must finish catch-up before its resident load. */
bool dc_camera_offscreen_pending(const dc_level_view_t *view, dc_chunk_coord_t coord);
/** Elect one eligible camera eviction after its worker request has been accepted. */
void dc_camera_offscreen_offer(dc_level_view_t *view, const dc_stream_result_t *snapshot,
    dc_chunk_owner_t owner, uint64_t write_generation);
/** Complete the election only after the matching resident save is acknowledged. */
void dc_camera_offscreen_saved(dc_level_view_t *view, const dc_stream_result_t *result);
/** Step an isolated owner or commit its confirmed state before camera return or shutdown. */
bool dc_camera_offscreen_service(dc_level_view_t *view, bool closing, char *err, uint32_t cap);
/** Release a single camera workspace after detaching its leases. */
void dc_camera_offscreen_release(dc_camera_offscreen_t *camera);
/** Release all camera workspaces after detaching the level leases. */
void dc_camera_offscreen_discard(dc_level_view_t *view);
/** Retain the durable election for retry after releasing a failed workspace load. */
void dc_camera_offscreen_retry(dc_camera_offscreen_t *camera);

#endif
