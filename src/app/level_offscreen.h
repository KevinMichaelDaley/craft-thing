#ifndef DC_APP_LEVEL_OFFSCREEN_H
#define DC_APP_LEVEL_OFFSCREEN_H

#include "level.h"
#include "offscreen.h"

typedef enum {
    DC_LEVEL_OFFSCREEN_UNMATCHED,
    DC_LEVEL_OFFSCREEN_HANDLED,
    DC_LEVEL_OFFSCREEN_FAILED
} dc_level_offscreen_result_t;

/** Reserve a cold structure owner and queue its source load into a workspace.
 * The caller keeps the workspace alive until the handoff completes or is canceled. */
bool dc_level_offscreen_begin(dc_level_view_t *view, dc_offscreen_t *workspace,
    dc_chunk_coord_t coord, uint64_t *generation, char *err, uint32_t cap);
/** Start from an acknowledged resident eviction, even when its archive is now absent. */
bool dc_level_offscreen_begin_disk(dc_level_view_t *view, dc_offscreen_t *workspace,
    dc_chunk_coord_t coord, dc_chunk_owner_t owner, dc_chunk_time_t time,
    uint64_t *generation, char *err, uint32_t cap);
/** Query the admitted clock and whether a checked write is already in flight. */
bool dc_level_offscreen_progress(dc_level_view_t *view, dc_chunk_coord_t coord,
    uint64_t generation, dc_chunk_time_t *time, bool *writing);
/** Queue an immutable confirmed replacement and suspend workspace ticks until the reply. */
bool dc_level_offscreen_publish(dc_level_view_t *view, dc_chunk_coord_t coord,
    uint64_t generation, char *err, uint32_t cap);
/** Abandon an unpublished owner when no disk replacement is in flight. */
bool dc_level_offscreen_cancel(dc_level_view_t *view, dc_chunk_coord_t coord,
    uint64_t generation, char *err, uint32_t cap);
/** Route a worker result by coordinate, operation, generation and durable owner. */
dc_level_offscreen_result_t dc_level_offscreen_result(dc_level_view_t *view,
    const dc_stream_result_t *result, char *err, uint32_t cap);
/** Check whether camera or transfer admission must wait on a workspace owner. */
bool dc_level_offscreen_owns(dc_level_view_t *view, dc_chunk_coord_t coord);
/** Report writes that shutdown must drain before committing a staged world. */
bool dc_level_offscreen_writing(const dc_level_view_t *view);
/** Release level lease bookkeeping without destroying the caller's workspaces. */
void dc_level_offscreen_discard(dc_level_view_t *view);

#endif
